# FTXUI Component Architecture

This document explains how the FTXUI frontend in `src/ui/` works: the concepts,
the data flow, and every function that matters, component by component.

## The big picture: FTXUI's model

FTXUI has **two separate trees** that this codebase uses deliberately:

1. **Component tree** — interactive things that can hold focus and receive events
   (`Button`, `Input`, `Checkbox`, `Radiobox`, `Container`, `Maybe`, `Modal`).
   Built once, kept alive.
2. **Element tree** — the pure visual output (`text`, `hbox`, `vbox`, `window`,
   `border`, `color`). Rebuilt *every single frame* from scratch.

The bridge between them: a `Component`'s `->Render()` produces an `Element`. FTXUI
runs one event loop (`screen.Loop`) that, on each frame, (a) routes input events
down the focused component path, then (b) calls the root renderer to produce a
fresh Element tree and diffs it to the terminal.

There is **no `onHover` in this codebase**. FTXUI's interaction model is *focus*,
not hover. A component is either focused (reachable via Tab/arrows, or just
clicked) or not. `EntryState::focused` is what every `transform` checks to draw
the highlight. A mouse *click* is what triggers `on_click`.

---

## Layer 1 — Coordinator: `FTXUIDisplay`

The thread-facing façade (implements the abstract `Display` interface). It does no
FTXUI itself — it owns one `GridView` and routes callbacks to whatever view is
"active".

- **`run(onCommand)`** (`FTXUIDisplay.cpp:28`) — the outer loop. Sets `m_grid`
  active, calls `m_grid.run(onCommand)` (blocks until the view exits), clears
  active. The `while (!m_quit)` lets you later swap between multiple views.
- **`onStateChanged()` / `onLog()`** (`:39`, `:46`) — called **from other
  threads** (heartbeat discovery, the logger). Under `m_routeMutex`, forwards to
  the active view. This is how a background network event turns into a redraw.
- **`quit()`** (`:53`) — sets the flag and calls `m_active->requestExit()` to
  break the FTXUI loop.
- The mutex + `m_active` pointer exist purely because FTXUI's screen isn't
  thread-safe; only one view touches the screen at a time.

## Layer 2 — View base: `View`

The thread-safety primitive (`View.cpp`). `m_app` is an `atomic<ftxui::App*>`, set
while the loop runs, null otherwise.

- **`redraw()`** (`View.cpp:15`) → `s->PostEvent(Event::Custom)`. This is *the*
  mechanism for "something changed off-screen, repaint." `PostEvent` is
  thread-safe; it wakes the loop, which re-renders.
- **`requestExit()`** (`:9`) → `s->Exit()`, breaks `screen.Loop`.

Key idea: **background threads never draw.** They only *post events* into the loop.
All actual rendering happens on the UI thread.

---

## Layer 3 — The main screen: `GridView::run` (the heart of everything)

This one function (`GridView.cpp:29`) wires the entire homescreen.

### 3a. Screen + refresh plumbing

```cpp
auto screen = App::Fullscreen();   // the terminal-owning screen
m_app = &screen;                   // publish it so other threads can PostEvent
```

**`m_refresh`** (`:36`) — a closure that does `screen.Post(...)`. `Post` queues a
job to run **on the UI thread**. The job calls `m_grid.rebuild()` +
`m_info.sync()` + `m_presets.sync()` then `PostEvent(Custom)`. It's posted rather
than run inline so a click handler can't destroy the very card it's executing
inside.

### 3b. Off-thread command dispatch

**`dispatch`** (`:54`) — the critical concurrency pattern. UI-triggered commands
(apply a preset, edit a field) do **blocking TCP I/O**. Running that inline would
freeze the loop. So:

```cpp
if (m_busy.exchange(true)) return;   // one op at a time; drop overlaps
std::thread([...]{ m_command(cmd); m_busy=false; m_refresh(); }).detach();
```

`m_busy` (atomic) both serializes commands over the shared `ESPClient` queue and
freezes selection while I/O is in flight. When the device responds, `m_refresh()`
posts the cache→UI resync.

### 3c. Wiring the child components via callbacks

Loosely-coupled pieces talk by the parent injecting behavior into children:

- `m_presets.setOnApply(...)` (`:69`) → turns a preset pick into
  `dispatch("setpreset N")`.
- `m_info.setOnCommand(dispatch)` (`:71`) → field edits become dispatched commands.
- `m_info.setPresetSection(...)` (`:77`) → **nests** the preset picker's component
  *inside* the info panel's focus tree (component, render fn, "is it active?"
  predicate).
- `m_grid.setAdditiveProvider(...)` (`:84`) → grid asks "should a click add to
  selection?" → true if modifier held or multi-mode on.
- `m_grid.setOnSelect(...)` (`:86`) → after a click changes selection, request a
  refresh.
- `m_grid.setSelectableProvider(...)` (`:90`) → clicks ignored while `m_busy`.

The pattern: children never know about `GridView`. They call provider/callback
`std::function`s the parent supplied. That's the whole decoupling strategy.

### 3d. The two button bars — `Menubar` and `Toolbar`

Both take `vector<MenubarItem>` where each item is
`{label, onClick, enabled?, active?, labelFn?}`. Idioms:

- **Toggle** (`Multi`, `:101`): `onClick` flips a bool; `active` returns that bool
  so `transform` lights it green.
- **Guarded** (`Select All`, `:103`): `if (m_busy) return;` inside onClick.
- **Disabled-when-empty** (`Clear`, `:104`): passes `hasSelection` as `enabled` →
  grays out and no-ops.
- **Two-click arm** (`RST`, `:122`): first click sets `m_armReset`, `labelFn` swaps
  the label to `"RST?"`, `active` lights it; second click fires
  `factoryreset confirm`. A confirm without a dialog.

### 3e. The component layout tree (built once)

```cpp
auto infoSlot = Maybe(m_info.component(), [this]{ return m_info.focusable(); });
auto layout = Container::Vertical({ menubar,
                Container::Horizontal({m_grid.component(), infoSlot, toolbar}) });
```

- **`Container::Vertical/Horizontal`** — group children and define Tab/arrow focus
  traversal order.
- **`Maybe(comp, predicate)`** — conditionally includes a component in the focus
  tree. When `focusable()` is false, Tab *skips over* the info panel entirely.
  Used everywhere (fold sections, preset picker) to keep focus off dead UI.

### 3f. The renderer (runs every frame)

**`Renderer(layout, lambda)`** (`:145`) — wraps the component tree; the lambda
builds the **Element** tree each frame. It calls `component->Render()` to embed
interactive pieces into the visual layout, and branches on `m_exec->isSelected()`
to show/hide the Info window. `window(title, body)`, `hbox`, `vbox`, `flex`,
`size(WIDTH, EQUAL, 40)` are all Element combinators — pure layout, recomputed
constantly.

### 3g. The modal popup

**`Modal(universePopup, &m_showUniverse)`** (`:191`) — overlays the universe
viewer, centered, when the bound bool is true. `universePopup` is its own
`Renderer`.

### 3h. Global key/mouse handling

**`CatchEvent(root, lambda)`** (`:193`) — intercepts events **before** they reach
children. Returning `true` consumes the event; `false` lets it fall through to
normal focus routing. Uses:

- Tracks `m_modifier` from `event.mouse().shift/control` on every mouse event (so
  cards know single vs multi-select) — runs *before* the card's own click handler.
- Mouse wheel → `m_info.scroll(±1)`.
- Character shortcuts (`u`, `m`, `a`, `c`, Esc).

### 3i. The 1-second ticker

**`std::thread ticker`** (`:235`) — every second calls `onStateChanged()`, which
posts a rebuild+redraw. Keeps the ping squares (online/offline) and
newly-discovered devices fresh even with no user input.

### 3j. Run and teardown

```cpp
screen.Loop(with_keys);   // BLOCKS here — this is the actual event loop
// after exit:
ticking=false; ticker.join(); m_refresh=nullptr; m_app=nullptr;
```

---

## Layer 4 — Individual interactive components

### `FixtureCard` — the click target (`FixtureCard.cpp`)

A card **is a `Button`**. `ButtonOption::Simple()`, then:

- **`opt.on_click`** = the closure the grid passed in.
- **`opt.transform`** (`:24`) = `[=](const EntryState& s) -> Element` — the
  per-frame draw. Everything is **captured by value** (`const int index`, `name`,
  etc.) because the grid rebuilds cards on every refresh — the closure must render
  the snapshot it was born with, not a dangling reference. Draws a bordered
  `vbox`; border goes green if `selected`. It deliberately ignores `s.focused`
  (`:46`) so no card looks "picked" just from keyboard focus — only real selection
  shows.

### `FixtureGrid::rebuild()` — regenerates all cards (`FixtureGrid.cpp:24`)

The "update" path for the grid. On each call:

1. Snapshots selection + `m_state.snapshot()`.
2. Assigns each fixture a **canonical flat index** in snapshot order (must match
   what `CommandExecutor` uses for `select N`).
3. Optional display-only sort by name (index preserved).
4. Builds one `onClick` per card (`:89`): checks `m_canSelect()` freeze →
   `m_exec->selectIndex(idx, additive)` → `m_onSelect()`.
5. `m_container->DetachAllChildren()` then re-adds cards grouped into
   `Container::Horizontal` rows of `m_cols`.

So the grid never mutates cards — it **throws them all away and rebuilds** every
refresh. State lives in `CommandExecutor`/`SharedState`, not in the widgets.

### `Menubar` / `Toolbar` factories (`Menubar.cpp`, `Toolbar.cpp`)

Identical structure, different layout (Horizontal vs Vertical; Toolbar draws
bordered square buttons). Each item → a `Button` whose:

- `on_click` guards on `enabled()` before calling `action`.
- `transform` picks the foreground color by priority: disabled → engaged
  (`active()`) → focused (`s.focused`) → idle, and uses `labelFn()` for dynamic
  labels (the `RST`→`RST?` swap).

### `InfoPanel` — the editable form (`InfoPanel.cpp`)

Built once in the constructor, then two lifecycle methods keep it in sync.

**Widgets and their events:**

- **`Input`** (name/universe/address/ssid/password): `InputOption.content =
  &m_name` binds the widget's text to a member string. `on_enter` fires the
  command. `transform = editBox` styles it (brightens `bgcolor` when
  `s.focused`). Numeric fields wrap the Input in `CatchEvent` (`:124`) that
  swallows non-digit characters. Password field sets `opt.password = true`.
- **`Checkbox`** (engine toggles, `:160`): `checked = &m_serial`, `on_change`
  fires `serialprint on/off`. Subtlety (`:159`): `sync()` writes the bound bool
  directly, which does *not* trigger `on_change`, so `on_change` only ever means a
  real user toggle.
- **Icon `Button`s** (`iconBtn`, `:141`): apply(✓)/revert(↻), glyph-only.
- **Section headers** (`sectionHdr`, `:203`): a `Button` whose `on_click` flips a
  `bool* open`; the arrow glyph `▾/▸` reflects `*open`.

**`buildContainer()`** (`:228`) — assembles the focus tree with nested `Maybe(...)`
gating each fold section by its open-flag, and `Maybe(m_singleContainer,
{m_active})` / `Maybe(m_multiContainer, {m_multi})` to switch between
single-fixture form and multi-selection listing.

**`sync()`** (`:355`) — the **update-from-model** method. Reads current selection +
a fresh `SharedState` snapshot, sets `m_active`/`m_multi`, mirrors engine bools
and (only when the *target fixture changes*, `:407`) reloads the editable text —
so the 1-second refresh doesn't clobber text you're mid-typing.

**`render()`** (`:422`) — builds the Element tree each frame. Three branches:
multi-selection collapsible blocks, zero/offline fallback (`infoListing`), or the
single editable form. Calls each widget's `->Render()` inline to place the live
component into the layout, and wraps everything in `focusPositionRelative +
yframe + vscroll_indicator` for scrolling.

**`applyEdits()` / `revertEdits()`** (`:303`) — apply diffs each field against its
`m_base*` baseline and fire only the changed commands; revert copies baseline back
into the bound strings.

### `PresetDropdown` (`PresetDropdown.cpp`)

Uses **`Radiobox`** (single-choice list) wrapped in
**`Collapsible(&m_label, Radiobox(opt), &m_open)`** — a built-in expandable
section. `onApply` (set by GridView) fires when a preset is chosen.

---

## The complete update cycle, end to end

**User clicks a card:**

1. `screen.Loop` gets a mouse event → `CatchEvent` records `m_modifier`, returns
   false → event routes to the focused card `Button` → its `on_click` runs.
2. `on_click` → `m_exec->selectIndex(idx, additive)` (mutates `CommandExecutor`
   selection) → `m_onSelect()` → `m_refresh()`.
3. `m_refresh` does `screen.Post(...)` → job runs on UI thread:
   `m_grid.rebuild()` (new cards, one now green) + `m_info.sync()` +
   `m_presets.sync()` → `PostEvent(Custom)`.
4. Loop wakes, re-runs the root Renderer → fresh Element tree (Info window now
   appears) → diffed to terminal.

**A field edit that hits the network:**

1. `Input.on_enter` → `applyEdits()` → `m_onCommand("setname X")` → GridView's
   `dispatch`.
2. `dispatch` sets `m_busy`, spawns a detached thread doing blocking TCP; loop
   stays responsive.
3. Thread finishes → `m_busy=false` → `m_refresh()` posts the resync → screen
   updates with the device's confirmed new state.

**Background device discovery:**

1. Heartbeat thread updates `SharedState`, calls
   `FTXUIDisplay::onStateChanged()` → routes to `GridView::onStateChanged()` →
   `screen.Post(rebuild+sync)` → `PostEvent`. New card appears. The 1s ticker does
   the same on a timer for ping liveness.

---

## `onStateChanged`: where it lives and what actually drives it

### Where it's defined (overrides)

| Location | What it does |
|---|---|
| `Display.h:55` | Base interface: `virtual void onStateChanged() {}` — no-op default (CLI ignores it). |
| `FTXUIDisplay.cpp:39` | Coordinator: under `m_routeMutex`, forwards to `m_active->onStateChanged()`. |
| `GridView.cpp:251` | The real work: `screen.Post([...]{ m_grid.rebuild(); m_info.sync(); m_presets.sync(); PostEvent(Custom); })`. |
| `View.h:27` | Fallback for any other view: just `redraw()`. |

### Where it's actually called

Only **one live call site**: `GridView.cpp:240`, inside the 1-second ticker thread
that `run()` spawns. `GridView` calls **its own** `onStateChanged()` directly (not
via `FTXUIDisplay`). So the "fresh data" path is currently **polling**: once a
second, unconditionally rebuild everything.

### The push path that's designed but not wired

The comments in `FTXUIDisplay.h` and `View.h` describe `onStateChanged` being
invoked from the heartbeat discovery thread. That push path **does not exist
today** — there are zero callers in `src/core` or `src/net`. The intended
plumbing sits **commented out** in `SharedState.cpp` (`addClient`/`setClientName`,
lines 61–103): they were meant to call a `triggerChange()` that notifies the
display, and only "when something actually changed" so the grid doesn't flicker on
every identical heartbeat packet. As a result, `FTXUIDisplay::onStateChanged` (the
coordinator override) is effectively dead code today.

To restore the push path, give `SharedState` a callback and call it from the
mutating methods:

```cpp
// SharedState
void setOnChange(std::function<void()> cb) { m_onChange = std::move(cb); }
void triggerChange() { if (m_onChange) m_onChange(); }
// ...in updateLastPing / addClient, after a real change: triggerChange();

// wiring (main.cpp, after makeDisplay):
state.setOnChange([&]{ display->onStateChanged(); });
```

The 1s ticker should stay regardless, because ping liveness (green/red square)
depends on wall-clock elapsed since the last ping, not on data changing.

---

## Adding a tab bar (switching between views)

The current design has **each view own its own `screen.Loop()`**
(`GridView::run` calls `App::Fullscreen()` and blocks). Switching views that way
tears down and rebuilds the whole screen on every switch: flicker, and each view
loses its focus/scroll/half-typed-text state.

The idiomatic FTXUI answer is the opposite: **one screen loop, one
`Container::Tab`**. All views stay alive simultaneously; the tab container only
renders and routes events to the selected child. Switching tabs is just changing
an `int`, and state is preserved for free.

### Step 1 — make each view expose a component, not a loop

```cpp
// ui/Panel.h — a tab-hostable view
namespace ui {
class Panel {
public:
    virtual ~Panel() = default;
    virtual ftxui::Component component() = 0; // interactive tree, incl. its own Renderer
    virtual void sync() {}                     // refresh from model (cache → widgets)
    virtual const char *title() const = 0;
};
}
```

`GridView` barely changes — everything from `auto layout = ...` through the final
`Renderer(...)` already builds a component. Stop creating the screen inside it and
return that renderer instead of calling `screen.Loop`:

```cpp
ftxui::Component GridView::component() {
    // ... existing wiring (menubar, toolbar, m_grid.component(), etc.) ...
    auto renderer = Renderer(layout, [&]{ /* unchanged render lambda */ });
    return renderer | Modal(universePopup, &m_showUniverse);
}
const char *GridView::title() const { return "Fixtures"; }
```

The Universe tab is nearly free — the existing `universeGrid(m_state, sel)` element
(used today in `universePopup`) becomes a `Renderer`-wrapped panel. The Command
tab is a new panel (an `Input` + scrollback of log lines).

### Step 2 — the host: one loop, one `Container::Tab`

```cpp
void HomeScreen::run(std::function<bool(const std::string &)> &onCommand) {
    auto screen = App::Fullscreen();
    m_app = &screen;

    int tab = 0;

    // Tab selector: reuse the existing Menubar — its `active` predicate already
    // lights the current item green.
    auto tabBar = Menubar({
        {"Fixtures", [&]{ tab = 0; }, nullptr, [&]{ return tab == 0; }},
        {"Command",  [&]{ tab = 1; }, nullptr, [&]{ return tab == 1; }},
        {"Universe", [&]{ tab = 2; }, nullptr, [&]{ return tab == 2; }},
    });

    // Only the child at index `tab` renders & gets events. All stay alive, so
    // switching preserves each view's state.
    auto content = Container::Tab({
        m_grid.component(),
        m_console.component(),
        m_universe.component(),
    }, &tab);

    auto layout = Container::Vertical({ tabBar, content });

    auto root = Renderer(layout, [&]{
        return vbox({
            tabBar->Render() | bgcolor(Color::RGB(30, 30, 40)),
            separator(),
            content->Render() | flex,   // Container::Tab renders only child[tab]
        }) | flex;
    });

    auto with_keys = CatchEvent(root, [&](Event e){
        if (e == Event::Tab)        { tab = (tab + 1) % 3; return true; }
        if (e == Event::TabReverse) { tab = (tab + 2) % 3; return true; }
        if (e == Event::Character('1')) { tab = 0; return true; }
        if (e == Event::Character('2')) { tab = 1; return true; }
        if (e == Event::Character('3')) { tab = 2; return true; }
        if (e == Event::Escape) { onCommand("exit"); screen.Exit(); return true; }
        return false;
    });

    screen.Loop(with_keys);   // the ONE loop for all tabs
    m_app = nullptr;
}
```

Two FTXUI facts that make this work:

- **`Container::Tab(children, &tab)`** keeps every child constructed and stateful,
  but on each frame only `children[tab]` is rendered and receives input.
  Switching = writing `tab`. No teardown, no flicker; focus/scroll/typed-text
  survive.
- **`->Render()` on the tab container** returns just the active child's element —
  that's why the `content->Render()` line works.

Caveat: `Event::Tab` is also how FTXUI moves focus *between widgets inside a view*.
If you bind global Tab to switch tabs, use a different chord for within-view focus
(or use number keys / `Ctrl+arrows` for tabs and leave `Tab` for focus).

### Step 3 — refresh & the background ticker

The host owns the `screen`, so it owns the ticker and the `Post`-based refresh, and
calls `sync()` on the panels:

```cpp
std::thread ticker([&]{
    while (ticking.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (auto *s = m_app.load())
            s->Post([&]{ m_grid.sync(); m_universe.sync(); s->PostEvent(Event::Custom); });
    }
});
```

Sync only the active tab (cheaper) or all of them (simpler — a background tab is
already correct when you switch to it). Since `Container::Tab` keeps them alive,
syncing all is safe. `FTXUIDisplay::onStateChanged()` (from the heartbeat thread)
routes to this host, which posts the same `sync()` job.

### Why not the loop-switch approach

You *can* keep the per-view-loop design and have `FTXUIDisplay::run` pick the next
view based on a key — less code up front, no `Panel` refactor. But you pay every
switch: full screen rebuild, lost per-view state (the command view forgets its
scrollback and half-typed line; the info panel loses its expanded sections).
`Container::Tab` is the reason FTXUI has a tab primitive at all.

---

## Note: duplicate / split `InfoPanel`

The monolithic `InfoPanel.cpp` is what `GridView` actually uses, but there are also
`infopanel/FixtureEditor.{cpp,h}`, `EngineSection.{cpp,h}`, and
`FixtureListing.{cpp,h}` with the *same* Input/Checkbox code (e.g.
`FixtureEditor.cpp:21` mirrors `InfoPanel.cpp:113`). These look like an in-progress
decomposition of the monolith on the `restructure` branch — the split-out sections
aren't wired into `GridView` yet. Confirm which is canonical before editing.
