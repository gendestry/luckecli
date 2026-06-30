# Display Components

The FTXUI UI is built from small, single-responsibility components. Each lives in
its own subfolder under `src/Display/Components/`, and include paths are
src-rooted (e.g. `"Display/Components/InfoPanel/InfoPanel.h"`) so files resolve
correctly regardless of their folder.

CMake uses `GLOB_RECURSE` for sources, so new `.cpp` files in these folders are
picked up automatically — no CMake edits needed when adding a component.

## Layout

```
Components/
  Toolbar/          Toolbar.{h,cpp}        — horizontal row of action buttons
  StatusBar/        StatusBar.h            — bottom status line (online/selected counts)
  PingIndicator/    PingIndicator.h        — per-fixture liveness square
  FixtureCard/      FixtureCard.{h,cpp}    — one clickable fixture in the grid
  PresetDropdown/   PresetDropdown.{h,cpp} — preset picker for the selected fixture
  UniverseGrid/     UniverseGrid.{h,cpp}   — DMX universe viewer popup content
  InfoPanel/        InfoPanel.{h,cpp}      — detail panel orchestrator (+ sub-components)
```

## InfoPanel

The detail panel for the current selection. It is split into focused
sub-components rather than one large class:

- **`InfoPanel`** — thin orchestrator. Decides editor-vs-listing based on the
  selection and delegates the actual fixture UI.
- **`FixtureEditor`** — the component for the single fixture being displayed:
  the editable name / universe / address inputs, an apply/revert bar, and the
  engine section. Edits stay local until **Apply** commits them (or Enter in any
  field), at which point only the changed fields are sent as shell commands and
  the typed values become the new baseline; **Revert** restores the baseline.
  Text fields only reload from the cache when the target fixture changes, so the
  1s refresh never clobbers a half-typed edit.
- **`EngineSection`** — the serial- and wireless-report toggles. Self-contained
  with its own command sink; each toggle commits on change.
- **`ApplyRevertBar`** — the ✓ (apply) / ↻ (revert) glyph buttons.
  Parameterized with apply/revert callbacks and a `dirty` predicate that drives
  the "edited" marker. Uses standard Unicode glyphs (U+2713, U+21BB) so they
  render in any terminal font (not Nerd Font private-use codepoints).
- **`FixtureListing`** — the read-only listing shown for zero/multi selection
  (and as a fallback). Reads live from the cached client state.
- **`InfoStyle.h`** — shared header-only helpers (`infoLabel`, `infoValue`,
  `findClient`, `editBox`) used across the InfoPanel sub-components.

## Building

Build inside the flake's dev shell so the link dependency (`libuuid`) is on the
path. In VS Code this happens automatically via direnv (`.envrc: use flake`);
from a terminal:

```
nix develop --command cmake --build build
```
