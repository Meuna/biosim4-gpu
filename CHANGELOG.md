# Changelog

## [Unreleased]

### Biosim4-gpu

#### Added

- Show an animated HUD (spinner, progress bar, survival/max/kill rates) on a TTY, keeping line-per-generation output when piped ([#169](https://github.com/Meuna/biosim4-gpu/pull/169))
- Add a corner barrier shape (an L of two arms) with a configurable quadrant ([#162](https://github.com/Meuna/biosim4-gpu/pull/162))

#### Changed

- Define barrier position and size as grid ratios in [0, 1] so layouts scale with the grid size ([#162](https://github.com/Meuna/biosim4-gpu/pull/162))

### Webapp

#### Added

- Report simulation engine failures in a dismissable bottom banner, with a blocking overlay for fatal startup failures ([#160](https://github.com/Meuna/biosim4-gpu/pull/160))
- Add barrier layout presets (cross, vertical split, bar cross, square, 5 dots, random) to the Barriers section ([#162](https://github.com/Meuna/biosim4-gpu/pull/162))
- Add a corner barrier kind to the Barriers control, with a quadrant selector ([#162](https://github.com/Meuna/biosim4-gpu/pull/162))
- Copy the simulation configuration to the clipboard from the Conf I/O row ([#168](https://github.com/Meuna/biosim4-gpu/pull/168))

#### Changed

- Require an exact Max neurons match to breed a loaded snapshot or running population ([#151](https://github.com/Meuna/biosim4-gpu/pull/151))
- Enlarge the simulation grid on small (phone) viewports by scaling the side margins with the viewport width ([#157](https://github.com/Meuna/biosim4-gpu/pull/157))
- Show the full-run survival rate with a min/now/max readout in a responsive sparkline that widens with each generation up to the grid width ([#167](https://github.com/Meuna/biosim4-gpu/pull/167))
- The survival spark line stay visible during evolve ([#172](https://github.com/Meuna/biosim4-gpu/pull/172))

#### Fixed

- Reflow the header and keep telemetry on-screen on small (phone) viewports ([#149](https://github.com/Meuna/biosim4-gpu/pull/149))
- Stop the expanded Brain view title from colliding with the version label ([#155](https://github.com/Meuna/biosim4-gpu/pull/155))
- Hide the agent hover card and re-enable Ctrl+click pinning when the cursor leaves the grid while Ctrl+hovering a border agent ([#155](https://github.com/Meuna/biosim4-gpu/pull/155))
- Stop the menu button from overlapping the grid hint on small (phone) viewports ([#161](https://github.com/Meuna/biosim4-gpu/pull/161))
- Keep the menu button on-screen when opening the panel on small (phone) viewports ([#164](https://github.com/Meuna/biosim4-gpu/pull/164))
- Remove the black focus outline on the Brain view's focused node ([#164](https://github.com/Meuna/biosim4-gpu/pull/164))

## [1.0.0] - 2026-06-14

### Biosim4-gpu

#### Added

- Reference simulator `biosim-ref` — deterministic single-threaded biosim4 engine
- GPU simulator `biosim-gpu` — OpenCL batch engine running the full per-step pipeline ([gh-26](https://github.com/Meuna/biosim4-gpu/issues/26))
- Full biosim4 sensor and action set on both CPU and GPU ([gh-20](https://github.com/Meuna/biosim4-gpu/issues/20))
- Configurable survival challenges
- Configurable barriers in four shapes
- Sexual reproduction with single-point crossover and fitness-biased parent selection
- Non-linear responsiveness curve (`responsiveness-curve-k`)
- `KILL_FORWARD` action with deferred death-marker resolution ([gh-22](https://github.com/Meuna/biosim4-gpu/issues/22))
- Per-generation statistics printed as aligned columns
- Snapshot checkpointing with `--snapshot-in`, `--snapshot-out`, and `--snapshot-interval` ([#105](https://github.com/meuna/biosim4-gpu/pull/105))
- Multi-level logging with `-v`/`-vv` verbosity ([gh-1](https://github.com/Meuna/biosim4-gpu/issues/1))
- Clean shutdown on SIGINT/SIGTERM ([gh-3](https://github.com/Meuna/biosim4-gpu/issues/3))
- Binaries build and run on Windows x64 (MSVC) ([#107](https://github.com/meuna/biosim4-gpu/pull/107))

### Webapp

#### Added

- Interactive web visualizer — Svelte single-page app rendering the live grid and agents on a canvas inside a Web Worker ([gh-44](https://github.com/Meuna/biosim4-gpu/issues/44))
- Scalar parameter configuration panel in the webapp ([#60](https://github.com/meuna/biosim4-gpu/pull/60))
- Survival-challenge configuration UI with a live grid overlay ([#65](https://github.com/meuna/biosim4-gpu/pull/65))
- Barrier configuration UI supporting up to 8 barriers with canvas hatching ([#67](https://github.com/meuna/biosim4-gpu/pull/67))
- Agent information panel showing live per-agent state ([#70](https://github.com/meuna/biosim4-gpu/pull/70))
- Brain Explorer view with force-directed neuron layout ([#75](https://github.com/meuna/biosim4-gpu/pull/75))
- Discrete speed selector and live FPS display in the play dock ([#95](https://github.com/meuna/biosim4-gpu/pull/95))
- "Evolve" webapp mode to advances full generations at maximum speed ([#97](https://github.com/meuna/biosim4-gpu/pull/97))
- Snapshot import/export in the webapp, including drag-and-drop of `.snap` ([#100](https://github.com/meuna/biosim4-gpu/pull/100))
- TOML configuration import/export in the webapp, including drag-and-drop of `.toml` files ([#100](https://github.com/meuna/biosim4-gpu/pull/100))
- Out-of-range parameter values round-trip through sliders without clamping ([#99](https://github.com/meuna/biosim4-gpu/pull/99))
