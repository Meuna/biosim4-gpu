# External Interface Control Document

**Purpose:** Define the external data formats, either with the user or across
package boundaries. Cross-package format must be agreed upon before any package
can read or write them. These formats are contracts: a change to either format
is a breaking change that requires coordinated updates across all consumers.

## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [CLI Flags and Configuration File Format](#2-cli-flags-and-configuration-file-format)
3. [Barrier Configuration](#3-barrier-configuration)
4. [Challenge Configuration](#4-challenge-configuration)
5. [Snapshot Binary Format](#5-snapshot-binary-format)

## 1. Goals and Non-Goals

### Goals

- Describe the main tool configuration interface: CLI flags and configuration files.
- Pin the cross-package snapshot format: mainly population genome, but also simulation
  parameters.
- Pin the `sim-stepper` output protocol and format.
- Pin the `sim-stepper` control protocol and format.

### Non-Goals

- Defining the in-memory representation of agents — that is the responsibility
  of `core`'s types and [`05-gpu-data-model.md`](05-gpu-data-model.md).

## 2. CLI Flags and Configuration File Format

### 2.1 Context

The simulation is driven by a collection of parameters that configure
everything defining a simulation: the grid size, the population size, the
genome length, mutation rate, challenge types, and dozens of other knobs.
biosim4 originally used a custom INI-style format parsed by hand.

The GPU port replaces it with a design that is:

- **Multi-source with precedence:** compiled-in defaults → TOML file → CLI
  flags. Each layer overrides the previous.
- **Table-driven:** each simulator's `main.c` owns a static entry table that
  is the source of truth for what parameters exist, their types, their names,
  and their defaults. CLI flags and TOML keys are derived from this table, not
  declared separately.
- Human-readable and human-editable.
- Supported by small, portable, C-compatible libraries available via vcpkg
  or vendored.

### 2.2 Decisions

| Concern | Choice | Status |
|---|---|---|
| Configuration file format | **TOML** via `tomlc17` | Decided |
| CLI parsing library | **argtable3** | Decided |
| Parameter model | Per-simulator table in each `main.c`; `params` package handles mechanics | Decided |

**TOML via `tomlc17`.** TOML is more expressive than INI (typed values,
arrays, grouped tables), still human-readable and diff-friendly, and
`tomlc17` is a minimal pure-C library. The familiarity argument for INI is
weak: the original format was not standard INI and required custom parsing
anyway. `tomlc17` is vendored in `third_party/tomlc17/` (two files, no
upstream CMake support).

**argtable3.** Portable (Linux, Windows, macOS), typed (int, double, string,
bool), available in vcpkg. Its declarative model — each argument is declared
as a typed struct field — aligns naturally with the shared parameter table.

### 2.3 Parameter Table — the `params` package

#### 2.3.1 Types

```c
typedef enum {
    PARAM_INT,
    PARAM_FLOAT,
    PARAM_BOOL,
    PARAM_STRING
} biosim_param_type_t;

typedef union {
    int         i;
    double      f;
    bool        b;
    const char* s;
} biosim_param_value_t;

typedef struct {
    const char*           name;          // in-memory key (getters/setters/introspection)
    const char*           table;         // TOML table name; NULL = top-level key
    biosim_param_value_t  default_value; // compiled-in default
    biosim_param_value_t  value;         // resolved value after three-pass
    biosim_param_type_t   type;
    bool                  is_set;        // true if overridden by TOML or CLI
    const char*           cli_long;      // NULL = auto ({table}-{name} or {name}); else override
    const char*           cli_short;     // NULL = no short flag; else e.g. "p" for -p
} biosim_param_entry_t;
```

**`name`** is the in-memory key used by getters, setters, and introspection.
Parameter names must be globally unique regardless of table assignment.

**`table`** controls TOML-file routing: `NULL` means the parameter is a
top-level key; a non-NULL string means the parameter lives under `[table]`
in the config file. Getter/setter lookups are unaffected — they still use
`name`.

**`cli_long`** is an optional override for the CLI long flag name (without
the `--` prefix). When `NULL`, the long flag is auto-generated as
`{table}-{name}` for table parameters, or just `{name}` for top-level
parameters. Set an override to shorten a verbose auto-generated flag
(e.g., `cli_long = "population"` on a `[simulation]` parameter keeps the
flag as `--population` rather than `--simulation-population`).

**`cli_short`** is an optional single-character short flag (e.g., `"p"` adds
`-p` as an alias). `NULL` means no short flag.

The `default_value` / `value` / `is_set` separation allows introspection
after resolution: the simulator can distinguish "user explicitly set this"
from "fell back to default", which is useful for logging and diagnostics.

#### 2.3.2 The params container

```c
typedef struct {
    biosim_param_entry_t* entries;
    size_t                count;
    size_t                capacity;
} biosim_params_t;
```

The container is a dynamically-sized array of entries. Each simulator's
`main.c` supplies the complete entry table at init time.

#### 2.3.3 `params` API

All functions below live in `biosim/params/params.h` (`params` package).
The setters, getters, and introspection functions operate on native C types
only and are unaware of tomlc17 or argtable3.

**Lifecycle:**

```c
// Initialize from caller-supplied entry table (entries are copied in)
biosim_status_t biosim_params_init(biosim_params_t* p,
                                   const biosim_param_entry_t* entries,
                                   size_t count);

// Release resources
void biosim_params_free(biosim_params_t* p);

// Three-pass resolution: defaults (in entries) → TOML (--config) → CLI flags
biosim_status_t biosim_params_parse(biosim_params_t* p,
                                    const char* progname,
                                    const char* version,
                                    int argc, char** argv);
```

`biosim_params_init` copies the caller-supplied table and resets `is_set` on
every entry. Each simulator's `main.c` owns the complete table — no shared
defaults, no extension step.

`biosim_params_parse` performs the full three-pass resolution (see §2.4) and
returns before the simulation loop. `progname` is shown in `--help` output;
`version` is shown in `--version` output. Both are plain strings so that
tests require no injected build-time macros.

**Setters (used by the TOML and CLI glue):**

```c
biosim_status_t biosim_params_set_int(biosim_params_t* p, const char* key, int val);
biosim_status_t biosim_params_set_float(biosim_params_t* p, const char* key, double val);
biosim_status_t biosim_params_set_bool(biosim_params_t* p, const char* key, bool val);
biosim_status_t biosim_params_set_string(biosim_params_t* p, const char* key, const char* val);
```

Each setter looks up the entry by `key`, verifies the type matches, writes
the value, and sets `is_set = true`. If the key is unknown, the setter
returns a warning status (not an error), to ease forward compatibility when
new parameters are added to the TOML file but not yet consumed by the code.

**Getters and introspection (used by simulation logic):**

```c
// Type-safe getters — abort on type mismatch (programming error)
int         biosim_params_get_int(const biosim_params_t* p, const char* key);
double      biosim_params_get_float(const biosim_params_t* p, const char* key);
bool        biosim_params_get_bool(const biosim_params_t* p, const char* key);
const char* biosim_params_get_string(const biosim_params_t* p, const char* key);

// Lookup
const biosim_param_entry_t* biosim_params_find(const biosim_params_t* p, const char* key);

// Iteration (for driving CLI and TOML glue)
size_t biosim_params_count(const biosim_params_t* p);
const biosim_param_entry_t* biosim_params_entry(const biosim_params_t* p, size_t index);
```

The iteration API is what makes the CLI and TOML integration table-driven:
the glue code loops over the entries, not over a hardcoded list of flags.

### 2.4 Three-pass resolution

Resolution happens at startup in each simulator's `main.c`, in strict order:

```
Pass 1 — biosim_params_init() copies entries; defaults are in the entry table
          → table is populated with compiled-in defaults

Pass 2 — TOML glue iterates the TOML file, calls biosim_params_set_*()
          → overrides defaults for any key present in the file

Pass 3 — CLI glue iterates parsed arguments, calls biosim_params_set_*()
          → overrides TOML (and defaults) for any flag on the command line
```

All three passes happen inside `biosim_params_parse`. After it returns, the
table is frozen and the simulation uses getters to read individual values.

### 2.5 Table-driven CLI generation

Every parameter in the table gets a CLI long flag. The long flag name is
resolved in priority order:

1. `e->cli_long` if non-NULL — explicit override (e.g., `"population"`)
2. `{table}-{name}` if `e->table != NULL` — auto-generated (e.g., `simulation-max-neurons`)
3. `{name}` — for top-level parameters with no table

An optional short flag (`-p`) is registered when `e->cli_short != NULL`.

The **one-line synopsis** shows only parameters from a hard-coded list of
prominent tables (currently `{"simulation"}`); parameters from other tables
are summarised with `...`. The full flag list for all tables appears in the
**glossary**, grouped by table.

```
Usage: biosim-stepper [-h] [--version] [--config=<path>] \
    [-s/--sim-name=<s>] [-p/--population=<n>] [--grid-size-x=<n>] ... 

  -h, --help               print help and exit
  --version                print version and exit
  --config=<path>          TOML config file

[simulation]
  -s, --sim-name=<s>
  -p, --population=<n>
  --grid-size-x=<n>
  ...
```

**Consequence: adding a simulation parameter = adding one entry to the
simulator's static entry table.** The CLI and TOML parsing pick it up
automatically with no changes to the `params` package.

### 2.6 Table-driven TOML loading

Same principle — the TOML glue iterates the parameter table, not a hardcoded
list of keys. When a parameter has a non-NULL `table`, the key is looked up
in the corresponding TOML sub-table; missing sections are silently skipped:

```c
for (size_t i = 0; i < biosim_params_count(&params); i++) {
    const biosim_param_entry_t* e = biosim_params_entry(&params, i);

    toml_datum_t src;
    if (e->table != NULL) {
        toml_datum_t subtab = toml_get(toptab, e->table);
        if (subtab.type != TOML_TABLE) continue;
        src = toml_get(subtab, e->name);
    } else {
        src = toml_get(toptab, e->name);
    }

    switch (e->type) {
        case PARAM_INT:
            if (src.type == TOML_INT64)
                biosim_params_set_int(&params, e->name, (int)src.u.int64);
            break;
        case PARAM_FLOAT:
            if (src.type == TOML_FP64)
                biosim_params_set_float(&params, e->name, src.u.fp64);
            else if (src.type == TOML_INT64)
                biosim_params_set_float(&params, e->name, (double)src.u.int64);
            break;
        case PARAM_BOOL:
            if (src.type == TOML_BOOLEAN)
                biosim_params_set_bool(&params, e->name, src.u.boolean);
            break;
        case PARAM_STRING:
            if (src.type == TOML_STRING)
                biosim_params_set_string(&params, e->name, src.u.s);
            break;
    }
}
```

## 3 Barrier Configuration

#### 3.1 Context

Barriers are static obstacles that block agent movement. They are declared
entirely in the TOML configuration file — there are no CLI flags for barriers,
and no barriers are created when no config file is supplied.

Barrier configuration uses a section-per-barrier layout rather than the
flat key-value model used for simulation parameters. The flat `biosim_param_entry_t`
table cannot express a variable number of named shapes, so barriers are parsed
by a dedicated module (`params/barriers.c`) that reads the TOML file independently.

#### 3.2 TOML Format

```toml
[barriers]
num-barriers = 3          # required; 0 or absent = no barriers

[barrier-1]
kind = "hbar"             # required; one of: hbar | vbar | square | circle
x = 64                    # optional; int — omit for random position
y = 32                    # optional; int — omit for random position
length = 40               # optional; number — omit for random dimension
width = 2                 # optional; number — omit for random dimension (bars only)

[barrier-2]
kind = "vbar"
x = 96
length = 30               # bar length along vertical axis

[barrier-3]
kind = "circle"
x = 50
y = 80
radius = 7.5              # alias for length on circle shapes; float accepted
```

Rules:

- No `[barriers]` section, or `num-barriers = 0`, produces zero barriers.
- `[barrier-N]` tables are numbered `1..num-barriers` consecutively.
  A missing table is a parse error (`BIOSIM_ERR_INVALID`).
- `kind` is the only required key; all position and dimension keys are optional.
- `radius` is accepted as an alias for `length` on `circle` shapes.
- Missing optional fields resolve to random values at simulation start (see §3.4).

#### 3.3 Shape Kinds and Parameters

| Kind | `length` | `width` | Description |
|---|---|---|---|
| `hbar` | horizontal extent (cells) | vertical thickness (cells) | Horizontal bar centred on (`x`, `y`) |
| `vbar` | vertical extent (cells) | horizontal thickness (cells) | Vertical bar centred on (`x`, `y`) |
| `square` | side length (cells) | ignored | Square centred on (`x`, `y`) |
| `circle` | radius (cells, float) | ignored | Disc centred on (`x`, `y`) |

`x` and `y` are the centre coordinates of the shape in grid cells (0-based,
origin at bottom-left). Out-of-bounds cells are clipped silently.

#### 3.4 Random Defaults

When a position or dimension is omitted, a value is drawn from the simulation's
`xorshift64` RNG seeded with `biosim_rng_seed(0, 0)`. This seed is fixed and
independent of any user-supplied seed, so random barrier layouts are fully
reproducible across runs with the same config file.

Default ranges (relative to grid dimensions `size_x`, `size_y`):

| Field | Default range |
|---|---|
| `x` | [`size_x/10`, `size_x*9/10`] |
| `y` | [`size_y/10`, `size_y*9/10`] |
| `hbar` / `vbar` length | [`size/4`, `size/2`] along the bar axis |
| `hbar` / `vbar` width | [1, 3] cells |
| `square` length (side) | [`size_x/8`, `size_x/4`] |
| `circle` length (radius) | [3.0, 10.0] cells |

#### 3.5 C Types and API

**`core` package** (`core/barriers.h`):

```c
typedef enum {
    BIOSIM_BARRIER_HBAR,
    BIOSIM_BARRIER_VBAR,
    BIOSIM_BARRIER_SQUARE,
    BIOSIM_BARRIER_CIRCLE,
} biosim_barrier_kind_t;

/* Sentinels for omitted fields */
#define BIOSIM_BARRIER_POS_UNSET ((int16_t)INT16_MIN)  /* x or y: random */
#define BIOSIM_BARRIER_DIM_UNSET (0.0F)                /* length or width: random */

typedef struct {
    biosim_barrier_kind_t kind;
    int16_t x;      /* centre x; BIOSIM_BARRIER_POS_UNSET = random */
    int16_t y;      /* centre y; BIOSIM_BARRIER_POS_UNSET = random */
    float   length; /* primary dimension; BIOSIM_BARRIER_DIM_UNSET = random */
    float   width;  /* bar thickness; BIOSIM_BARRIER_DIM_UNSET = random */
} biosim_barrier_spec_t;

/* Place all n specs onto grid as BIOSIM_GRID_BARRIER cells.
 * rng_state is advanced in-place; same initial state → same layout. */
biosim_status_t biosim_barriers_place(biosim_grid_t *grid,
                                      const biosim_barrier_spec_t *specs, int n,
                                      uint64_t *rng_state);
```

**`params` package** (`params/barriers.h`):

```c
/* Parse barrier specs from a TOML file.
 * Returns BIOSIM_OK with *n_out = 0 when path is NULL or [barriers] is absent.
 * *specs_out is heap-allocated; caller must free(). */
biosim_status_t biosim_barrier_params_load(const char *toml_path,
                                           biosim_barrier_spec_t **specs_out,
                                           int *n_out);
```

#### 3.6 Integration Contract

Barriers are placed on the grid **before** agents are spawned. This is
enforced inside `biosim_context_create`: it calls `biosim_barriers_place` on
the zero-filled grid, then places agents via `biosim_grid_find_empty`, which
already treats `BIOSIM_GRID_BARRIER` cells as occupied. The simulator is
therefore guaranteed that no agent starts inside a barrier cell.

The caller's responsibility:

1. Parse barrier specs from the TOML file with `biosim_barrier_params_load`.
2. Pass the resulting array and count to `biosim_context_create` (or the
   simulator's own create function, e.g. `biosim_stepper_create`).
3. `free()` the spec array after `context_create` returns.

## 4. Challenge Configuration

### 4.1 Context

Each simulation run applies a single survival challenge that determines which
agents reproduce. The challenge kind and its parameters are specified in the
TOML config file under a `[challenge]` section. Challenge parameters are
declared as standard `biosim_param_entry_t` rows in each simulator's entry
table (table = `"challenge"`); the three-pass resolution described in §2.4
applies, so any challenge parameter can also be overridden via CLI flags.

After `biosim_params_parse`, each simulator calls
`biosim_challenge_spec_from_params` (from the `params` package) to convert
the resolved parameter values into a `biosim_challenge_spec_t`. The spec is
then stored in `biosim_context_t.challenge` and passed to
`biosim_challenge_eval` at the end of each generation.

### 4.2 TOML Format

```toml
[challenge]
kind = "x_band"   # required; snake_case kind name
x-min = 0.5       # kind-specific params; omit to use defaults
x-max = 1.0
mirror = false
```

The `kind` field is required. All other keys are kind-specific and fall back to
per-simulator compiled-in defaults when absent.

### 4.3 Challenge Kinds

#### Parameterised kinds

| Kind | Parameters | Description |
|---|---|---|
| `x_band` | `x-min`, `x-max` (float, fraction of grid), `mirror` (bool) | Agents survive if `x ∈ [x_min·W, x_max·W)`. With `mirror = true` the mirrored band `[(1−x_max)·W, (1−x_min)·W)` also passes. |
| `disc` | `x`, `y`, `radius` (float, fractions), `weighted` (bool) | Agents within `radius` of centre (`cx·W`, `y·H`). Weighted: score = `(r − d) / r`; unweighted: score = 1. |
| `corners` | `radius` (float, fraction), `weighted` (bool) | Agents within `radius` of any of the four grid corners. |
| `neighbor_count` | `radius`, `min-n`, `max-n` (floats), `exclude-border` (bool) | Agents not on border whose occupied-cell neighbour count within `radius` is in `[min_n, max_n]`. |
| `center_sparse` | `x`, `y`, `outer-r`, `inner-r`, `min-n`, `max-n` (floats), `weighted` (bool) | Agents within `outer_r` of centre whose neighbourhood within `inner_r` has agent count in `[min_n, max_n]`. |
| `near_barrier` | `radius` (float, fraction) | Survive if within `radius·W` of any barrier centre. Score = `1 − dist / radius`. |

#### Dedicated kinds (no kind-specific parameters)

| Kind | Description |
|---|---|
| `against_wall` | Survive if at grid border at generation end. |
| `migrate_distance` | Everyone survives; score = `|loc − birth_loc| / max(W, H)`. |
| `touch_any_wall` | Survive if `challenge_bits ≠ 0` (set each step the agent touches a border). |
| `radioactive_walls` | Deaths handled each step by a probabilistic wall-proximity kill; evaluator returns `{passed: true, score: 1.0}` for all living agents. |
| `pairs` | Survive if agent has exactly one 8-connected neighbour that itself has no other neighbours. |
| `location_sequence` | Score = number of barrier centres visited in sequence / 32. Requires `challenge_bits` step tracking. |
| `altruism` | **Placeholder** — blocked on `genome_similarity`; evaluator returns `{false, 0.0f}`. |

### 4.4 Integration Contract

1. Each simulator declares challenge parameter entries in its `sim_params[]`
   table with table = `"challenge"`.
2. After `biosim_params_parse`, call `biosim_challenge_spec_from_params` to
   build the spec.
3. Assign the spec to `sim.challenge` (where `sim` is the `biosim_context_t`).
4. Each simulation step, call `biosim_challenge_step(&sim.challenge, &sim, step, steps_per_gen)`
   after agent movement and signal fade. Step-hook kinds update `challenge_bits` or kill
   agents in-place.
5. At the end of each generation, iterate over agents and call
   `biosim_challenge_eval(&sim.challenge, agent_idx, &sim)` per agent to determine
   survivors.

## 5. Snapshot Binary Format

### 5.1 Context

A snapshot captures the **survivor genomes** of a completed generation — the
evolutionary state needed to resume or replay a run. It is deliberately
minimal: world parameters (grid size, challenge, barriers, mutation rate, …)
are not stored; users can supply them via a companion TOML config file or use
the compiled defaults.

Snapshots are:

- Written by simulators after challenge evaluation and before reproduction,
  at the configured interval.
- Read by simulator to resume from the last stored generation.
- Read by viz (future) to render a generation movie.

The format is binary (little-endian) throughout. Text encoding would be
impractically large at high population counts.

### 5.2 File Layout

```
┌────────────────────────────────┐
│ FILE HEADER  (32 bytes)        │
├────────────────────────────────┤
│ GENERATION RECORD 0            │
│ GENERATION RECORD 1            │
│ …                              │
└────────────────────────────────┘
```

#### File Header (32 bytes)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 4 | magic | `0x42 0x53 0x4D 0x34` ("BSM4") |
| 4 | 2 | format_version | `BIOSIM_SNAP_FORMAT_VERSION` (starts at 1) |
| 6 | 2 | schema_version | `BIOSIM_IO_SCHEMA_VERSION` |
| 8 | 2 | num_sensors | `BIOSIM_NUM_SENSORS` at write time |
| 10 | 2 | num_actions | `BIOSIM_NUM_ACTIONS` at write time |
| 12 | 2 | genome_max_len | `sim->genome.max_len` at write time |
| 14 | 1 | max_neurons | `sim->nnet.max_neurons` at write time |
| 15 | 1 | reserved | zero |
| 16 | 4 | generation_count | number of records; 0 = unknown / streaming |
| 20 | 12 | reserved | zeros |

`format_version` is bumped when the on-disk record layout changes.
`schema_version` (`BIOSIM_IO_SCHEMA_VERSION` in `core/io_catalogue.h`) is
bumped when the sensor/action catalogue indices change, which would silently
corrupt gene interpretation. A mismatch is fatal on load.

#### Generation Record

Let `n` = `n_survivors` and `L` = `genome_max_len` (from file header).

| Offset | Size | Field | Type | Notes |
|---|---|---|---|---|
| 0 | 8 | record_size | uint64_t | total bytes including this field |
| 8 | 4 | gen | uint32_t | generation index |
| 12 | 4 | n_survivors | uint32_t | |
| 16 | 8 | gen_rng | uint64_t | gen_rng at collect-survivors time |
| 24 | n × 2 | genome_length | uint16_t[] | active gene count per survivor |
| 24 + n×2 | L × n × 2 | genome_conn | uint16_t[] | SoA: gene-slot-major |
| 24 + n×2 + L×n×2 | L × n × 2 | genome_wgt | int16_t[] | SoA: gene-slot-major |

The genome arrays use the same transposed SoA layout as `biosim_genome_t`:
gene slot `j` of survivor `s` is at index `j * n_survivors + s`.

`gen_rng` is captured after `biosim_generation_collect_survivors` and
**before** `biosim_generation_reproduce`. Restoring `sim->gen_rng` to this
value before calling reproduce yields a deterministic replay.

#### Approximate sizes

At defaults (pop = 3000, 750 survivors at 25%, genome_max_len = 24):
- ≈ 72 KB per generation record
- 1000 generations at interval 1 → ≈ 70 MB; at interval 10 → ≈ 7 MB

### 5.3 "Starting with survivors" restore pattern

On restore, the simulator loads snapshot survivor genomes into `sim->genome`
slots 0..n−1 and calls `biosim_generation_reproduce` exactly as it would at a
normal generation boundary. The main loop then proceeds identically to a
fresh run:

```
biosim_snapshot_load_last(f, &hdr, &sim, &n_surv, &gen_idx, &gen_rng)
sim.gen     = gen_idx
sim.gen_rng = gen_rng
survivors   = [0, 1, ..., n_surv-1]
biosim_generation_reproduce(&sim, survivors, NULL, n_surv)   /* uniform selection */
sim.gen++
```

No special code path in the simulation loop is required.
