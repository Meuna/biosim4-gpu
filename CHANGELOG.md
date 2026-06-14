# Changelog

## [Unreleased]

### Webapp

#### Fixed

- Reflow the header and keep telemetry on-screen on small (phone) viewports ([#149](https://github.com/Meuna/biosim4-gpu/pull/149))

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
