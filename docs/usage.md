# Usage

## `biosim-ref` — CPU reference simulator

```sh
biosim-ref [--config <path>] [OPTIONS]
```

With no arguments, the simulator runs with compiled-in defaults.

```sh
biosim-ref --config my_run.toml
biosim-ref --config my_run.toml --pop 5000 --max-gen 500
```

## `biosim-gpu` — OpenCL GPU simulator

### Prerequisites

An OpenCL runtime must be installed. For an easy smoke-test on Linux, install
[POCL](http://portablecl.org/):

```sh
sudo apt install pocl-opencl-icd
```

POCL provides an OpenCL 3.0 CPU driver that does not require physical GPU hardware.
See [`docs/build-opencl.md`](build-opencl.md) for full details.

### Running

```sh
biosim-gpu [--config <path>] [OPTIONS]
```

```sh
biosim-gpu --pop 4096 --grid-size-x 128 --grid-size-y 128
biosim-gpu --platform 0 --device 1    # select a specific OpenCL device
```

### Kernel filesystem override

Place a `.cl` file with the same name as an embedded kernel alongside the binary
to override it without rebuilding:

```sh
cp packages/sim-gpu/kernels/k1_sensors.cl build/debug/packages/sim-gpu/
biosim-gpu   # loads k1_sensors.cl from the binary directory
```

### OpenCL parameters

| CLI flag | TOML `[opencl]` key | Default | Description |
|----------|---------------------|---------|-------------|
| `--platform <n>` | `platform-index` | 0 | OpenCL platform index |
| `--device <n>` | `device-index` | 0 | OpenCL device index within the platform |

## `biosim-gpu-bench` — pipeline profiler

A developer-only benchmark for the GPU pipeline. It is built but **not
installed**; run it from the build tree. It creates the command queue with
`CL_QUEUE_PROFILING_ENABLE`, drives the pipeline (upload → K1–K5 × steps →
download) without CPU-side genetics, and prints per-kernel GPU time,
host↔device transfer time, and throughput. At startup it prints an info dump
(version, OpenCL platform/device, key performance params) — add `-v` to list
every parameter value — and shows a live spinner/progress bar during the timed
run on a TTY.

```sh
build/debug/packages/sim-gpu/biosim-gpu-bench --max-gen 20
```

It defaults to a large brain (population 8192, 64 genes, 32 neurons) to stress
the feedforward kernel; all simulation/genome/challenge flags from `biosim-gpu`
are accepted. The number of timed generations is taken from `--max-gen`
(`max-generations`), plus:

| CLI flag | Default | Description |
|----------|---------|-------------|
| `--max-gen <n>` | 20 | timed generations |
| `--warmup <n>` | 2 | untimed warm-up generations (driver JIT, first-touch) |
| `-v` | off | verbose info dump: list every parameter value |

For driver-level traces, run the unmodified binary under a profiler — e.g.
`nsys profile build/debug/packages/sim-gpu/biosim-gpu-bench` — which lists the
five `k_*` kernels in its timeline without any code change.

## Webapp

The webapp is a static Svelte SPA. After `cmake --build --preset webapp` the
bundled output lives in `packages/webapp/dist/`.

### Serving locally

Any static file server works. The quickest option with no extra install:

```sh
python3 -m http.server 8080 --directory build/webapp/packages/webapp/dist
```

Then open `http://localhost:8080` in a browser.

### Dev server

For active development with hot-reload:

```sh
cmake --build --preset webapp --target dev
```

Opens a Vite dev server at `http://localhost:5173`.

## Parameters

Parameters are resolved in three passes: compiled-in defaults → TOML
file (`--config`) → CLI flags. Each layer overrides the previous.

### General

| CLI flag | TOML key | Default | Description |
|----------|----------|---------|-------------|
| `-v`, `--verbose` | — | 0 | Verbosity: `-v` → INFO, `-vv` → DEBUG |
| `--config <path>` | — | — | TOML configuration file |

### Simulation

| CLI flag | TOML `[simulation]` key | Default | Description |
|----------|------------------------|---------|-------------|
| `-p`, `--pop <n>` | `population` | 3000 | Agent count |
| `-x`, `--grid-size-x <n>` | `grid-size-x` | 128 | Grid width (cells) |
| `-y`, `--grid-size-y <n>` | `grid-size-y` | 128 | Grid height (cells) |
| `--steps-per-gen <n>` | `steps-per-gen` | 300 | Steps per generation |
| `--max-gen <n>` | `max-generations` | 1000 | Number of generations to run |

> **Performance note:** `population`, `steps-per-gen`, `grid-size-x`,
> and `grid-size-y` are the primary throughput knobs. The stepper
> scales linearly with `population × steps-per-gen`.
> `grid-size-x × grid-size-y` affects memory and neighborhood sensor
> cost.

### Genome

| CLI flag | TOML `[genome]` key | Default | Description |
|----------|---------------------|---------|-------------|
| `--max-genes <n>` | `max-genes` | 24 | Maximum genes per agent |
| `--max-neurons <n>` | `max-neurons` | 5 | Maximum hidden neurons per agent |
| `--point-mut-rate <f>` | `point-mutation-rate` | 0.001 | Per-gene point mutation probability |
| `--genome-sexual-reproduction` | `sexual-reproduction` | false | Two-parent crossover instead of single-parent copy |
| `--genome-choose-parents-by-fitness` | `choose-parents-by-fitness` | false | Bias parent selection toward higher-score survivors |

> **Performance note:** `max-genes` and `max-neurons` set fixed
> allocation sizes for all agents. Larger values increase memory and
> feedforward compute proportionally.

### Sensors

| CLI flag | TOML `[sensors]` key | Default | Description |
|----------|----------------------|---------|-------------|
| `--sensors-los-range <n>` | `los-range` | 16 | Default long-probe sensor range (cells) |
| `--sensors-sensor-radius <n>` | `sensor-radius` | 2 | Radius for radial sensors |

### Actions

| CLI flag | TOML `[actions]` key | Default | Description |
|----------|----------------------|---------|-------------|
| `--enable-kill` | `enable-kill` | false | Enable `KILL_FORWARD` action |
| `--resp-curve-k <k>` | `responsiveness-curve-k` | 2.0 | Shape factor for the responsiveness curve; 0.0 = linear (identity), 2.0 = original biosim4 sigmoidal shape |

### Snapshot

| CLI flag | TOML `[snapshot]` key | Default | Description |
|----------|----------------------|---------|-------------|
| `--snapshot-in <path>` | `in` | — | Restore from last generation in this file |
| `--snapshot-out <path>` | `out` | — | Write survivor snapshots to this file |
| `--snapshot-interval <n>` | `interval` | 0 | Write every N generations (0 = final generation only); the final generation is always written |

The final generation is always recorded. If the run is halted by a signal
(`SIGINT`/`SIGTERM`), the last completed generation's survivors are written
before the file is closed, so an interrupted run can still be resumed with
`--snapshot-in`.

## TOML configuration

```toml
[simulation]
population      = 3000
grid-size-x     = 128
grid-size-y     = 128
steps-per-gen   = 300
max-generations = 1000

[genome]
max-genes            = 24
max-neurons               = 5
point-mutation-rate       = 0.001
sexual-reproduction       = false
choose-parents-by-fitness = false

[sensors]
los-range     = 16
sensor-radius = 2

[actions]
enable-kill              = false
responsiveness-curve-k   = 2.0

[challenge]
kind = "x_band"
# kind-specific parameters (see Challenge types below)

[snapshot]
out      = "run.snap"
interval = 10

# Barriers are declared with separate tables (see Barrier types below)
```

## Challenge types

Set `[challenge] kind = "<name>"` to select a challenge. All other
`[challenge]` keys are optional and fall back to compiled-in defaults
when absent.

### Parameterized challenges

#### `x_band`

Agents survive if their x-coordinate falls within `[x_min·W, x_max·W)`,
where W is the grid width.

```toml
[challenge]
kind   = "x_band"
x-min  = 0.5     # fraction of grid width — left edge of survival band
x-max  = 1.0     # fraction of grid width — right edge of survival band
mirror = false   # if true, the band is inverted: agents survive *outside* [x_min·W, x_max·W)
```

#### `disc`

Agents survive if within `radius` of the centre point (`x·W`, `y·H`).

```toml
[challenge]
kind     = "disc"
x        = 0.5      # centre x, fraction of grid width
y        = 0.5      # centre y, fraction of grid height
radius   = 0.333    # fraction of grid width
weighted = true     # if true, score = (r − dist) / r; else score = 1
```

#### `corners`

Agents survive if within `radius` of any of the four grid corners.

```toml
[challenge]
kind     = "corners"
radius   = 0.333    # fraction of grid width
weighted = true
```

#### `neighbor_count`

Agents survive if their occupied-cell neighbour count within `radius`
is in `[min_n, max_n]`.

```toml
[challenge]
kind           = "neighbor_count"
radius         = 0.333
min-n          = 5.0
max-n          = 8.0
exclude-border = false   # if true, border agents never survive
```

#### `center_sparse`

Agents survive if within `outer_r` of the centre and their
neighbourhood within `inner_r` has agent count in `[min_n, max_n]`.

```toml
[challenge]
kind     = "center_sparse"
x        = 0.5
y        = 0.5
outer-r  = 0.25     # fraction of grid width
inner-r  = 0.012    # fraction of grid width
min-n    = 5.0
max-n    = 8.0
weighted = true
```

#### `near_barrier`

Agents survive if within `radius·W` of any barrier centre.

```toml
[challenge]
kind   = "near_barrier"
radius = 0.333    # fraction of grid width; score = 1 − dist/radius
```

#### `location_sequence`

Score is the number of barrier centres visited in sequence divided by
32. Requires barriers to be configured.

```toml
[challenge]
kind = "location_sequence"
```

### Fixed challenges (no parameters)

| Kind | Behaviour |
|------|-----------|
| `against_wall` | Survive if at the grid border at generation end |
| `migrate_distance` | All agents survive; score = `|loc − birth_loc| / max(W, H)` |
| `touch_any_wall` | Survive if `challenge_bits ≠ 0` — set each step the agent touches a border |
| `radioactive_walls` | Wall-proximity kills applied each step; all living agents pass at evaluation |
| `pairs` | Survive if agent has exactly one 8-connected neighbour that itself has no other neighbours |
| `altruism` | **Placeholder** — always returns `{false, 0.0f}` |

## Barrier types

Barriers are declared in the TOML file as numbered tables. They are
placed on the grid before agents spawn, so agents never start inside
a barrier cell.

```toml
[barriers]
num-barriers = 2

[barrier-1]
kind   = "hbar"    # required: hbar | vbar | square | circle | corner
x      = 0.5       # centre x (grid ratio 0..1); omit for random
y      = 0.25      # centre y (grid ratio 0..1); omit for random
length = 0.3       # extent along bar axis (ratio); omit for random
width  = 0.02      # thickness perpendicular to bar axis (ratio); omit for random

[barrier-2]
kind   = "circle"
x      = 0.8
y      = 0.8
radius = 0.06      # alias for length on circle shapes
```

No `[barriers]` section (or `num-barriers = 0`) produces zero barriers.

### Shape parameters

| Kind | `length` | `width` | Description |
|------|----------|---------|-------------|
| `hbar` | horizontal extent | vertical thickness | Horizontal bar centred on (`x`, `y`) |
| `vbar` | vertical extent | horizontal thickness | Vertical bar centred on (`x`, `y`) |
| `square` | side length | ignored | Square centred on (`x`, `y`) |
| `circle` | radius | ignored | Disc centred on (`x`, `y`) |
| `corner` | arm length | arm thickness | L-shape with its junction at (`x`, `y`); see `quadrant` |

A `corner` is an L of one horizontal and one vertical arm meeting at the
junction (`x`, `y`). Its `quadrant` selects the arm directions (origin
bottom-left, y up): `ne` → right+up, `nw` → left+up, `se` → right+down,
`sw` → left+down. Defaults to `ne`; ignored by other kinds.

```toml
[barrier-3]
kind     = "corner"
x        = 0.3
y        = 0.3
length   = 0.2     # length of each arm
width    = 0.02    # arm thickness
quadrant = "ne"    # ne | nw | se | sw
```

All coordinates and dimensions are **grid ratios in [0, 1]**, so a layout
scales with the grid size. `x`/`y` are fractions of grid width/height with
origin at the bottom-left; `length`/`width` are fractions of the smaller grid
axis (`min(W, H)`). They resolve to cells against the target grid; out-of-bounds
cells are clipped silently.

When position or dimension is omitted, a value is drawn from the
simulation RNG seeded with `biosim_rng_seed(0, 0)` — the same seed
every run, so random barrier layouts are reproducible.
