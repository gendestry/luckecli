# Display Components

The FTXUI UI is built from small, single-responsibility components living under
`src/Display/Components/`. Include paths are src-rooted (e.g.
`"Display/Components/InfoPanel/InfoPanel.h"`) so files resolve regardless of
folder. CMake uses `GLOB_RECURSE`, so new `.cpp` files are picked up
automatically — no CMake edits when adding a component.

## Two kinds of component

**Stateless render helpers** — free functions that take inputs and return an
`Element`/`Component`. No persistent state; rebuilt every frame.

**Stateful widgets** — classes that own ftxui components built *once* over
member-backed storage. They share a uniform contract:

- `component()` — the ftxui component to mount in the layout (built once)
- `sync()` — refresh backing storage from shared state (UI thread only)
- `render()` — produce this frame's `Element`
- `setOnX(cb)` — callbacks out to the host

Two rules for stateful widgets: take `CommandExecutor*&` (reference to the
pointer, since `exec` is bound *after* construction via `bindCommandSource`), and
build the ftxui component once in the constructor — `sync()` only mutates backing
storage, it never rebuilds.

## Catalog

### Stateless render helpers

| File | Symbol | Used for |
|---|---|---|
| `PingIndicator.h` | `pingSquare(online)` | Green/red liveness ■ on each card |
| `StatusBar.h` | `statusBar(online, selected)` | Bottom bar: counts (left) + key hints (right) |
| `FixtureCard.{h,cpp}` | `FixtureCard(data, onClick)` + `FixtureCardData` | One clickable fixture tile |
| `Toolbar.{h,cpp}` | `Toolbar(items)` + `ToolbarItem` | Top action row (Multi / Select All / Clear / …) |
| `UniverseGrid.{h,cpp}` | `universeGrid(state, selection)` | 16×32 DMX map in the Universe popup |
| `InfoPanel/FixtureListing.{h,cpp}` | `fixtureListing(state, selection)` | Read-only list (0 / multi-selection fallback) |
| `InfoPanel/InfoStyle.h` | `infoLabel`/`infoValue`/`editBox`/`findClient` | Shared styling helpers for the InfoPanel pieces |

### Stateful widgets

| File | Class | Used for |
|---|---|---|
| `Display/FixtureGrid.{h,cpp}`* | `FixtureGrid` | Scrollable grid of `FixtureCard`s; rebuilds cards from state, handles click/selection |
| `Components/InfoPanel.{h,cpp}` | `InfoPanel` | Detail panel for the selected fixture; composes the sub-components below |
| `Components/PresetDropdown.{h,cpp}` | `PresetDropdown` | Collapsible preset picker (single selection) |

\* `FixtureGrid` is the one widget that lives in `Display/` rather than
`Components/`.

## InfoPanel

The detail panel for the current selection. It is split into focused
sub-components rather than one large class:

- **`InfoPanel`** — thin orchestrator. Picks editor-vs-listing based on the
  selection and delegates the actual fixture UI.
- **`FixtureEditor`** — the editable form for the single displayed fixture:
  name / universe / address inputs, an apply/revert bar, and the engine section.
  Edits stay local until **Apply** commits them (or Enter in any field), at which
  point only changed fields are sent as shell commands and the typed values
  become the new baseline; **Revert** restores it. Fields only reload from cache
  when the target fixture changes, so the 1s refresh never clobbers a half-typed
  edit.
- **`EngineSection`** — the serial- and wireless-report toggles. Self-contained
  with its own command sink; each toggle commits on change.
- **`ApplyRevertBar`** — the ✓ (apply) / ↻ (revert) glyph buttons.
  Parameterized with apply/revert callbacks and a `dirty` predicate that drives
  the "edited" marker. Uses standard Unicode glyphs (U+2713, U+21BB) so they
  render in any terminal font (not Nerd Font private-use codepoints).
- **`FixtureListing`** — the read-only listing for zero/multi selection.
- **`InfoStyle.h`** — shared header-only styling helpers.

## How they nest

```
GridView
├── Toolbar            (top)
├── FixtureGrid        (body) ──> FixtureCard ──> pingSquare
├── InfoPanel          (right side, when 1 selected)
│   ├── FixtureEditor ──> EngineSection, ApplyRevertBar
│   ├── PresetDropdown
│   └── FixtureListing (fallback)
├── StatusBar          (bottom)
└── UniverseGrid       (popup, toggled with 'u')
```

## Building

Build inside the flake's dev shell so the link dependency (`libuuid`) is on the
path. In VS Code this happens automatically via direnv (`.envrc: use flake`);
from a terminal:

```
nix develop --command cmake --build build
```
