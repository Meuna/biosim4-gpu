# External Interface Control Document

**Purpose:** Define the external data formats, either with the user or across
package boundaries. Cross-package format must be agreed upon before any package
can read or write them. These formats are contracts: a change to either format
is a breaking change that requires coordinated updates across all consumers.

## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [CLI Flags and Configuration File Format](#2-cli-flags-and-configuration-file-format)
3. [Snapshot Binary Format](#3-snapshot-binary-format)

## 1. Goals and Non-Goals

### Goals

- Describe the main tool configuration interface: CLI flags and configuration files.
- Pin the cross-package snapshot format: mainly population genome, but also simulation
  parameters.
- Pin the `sim-stepper` output protocol and format.
- Pin the `sim-stepper` control protocol and format.

### Non-Goals

- Defining the in-memory representation of parameters or agents — that is
  the responsibility of `core`'s types and
  [`05-gpu-data-model.md`](05-gpu-data-model.md).

## 2. CLI Flags and Configuration File Format

### 2.1 Context

The simulation is driven by a collection of flags that configure everything
defining a simulation: the grid size, the population size, the genome length,
mutation rate, challenge types, and dozens of other knobs. biosim4 originally
used a custom INI-style format parsed by hand.

The GPU port replaces it with a design that is:

- **Multi-source with precedence:** compiled-in defaults → TOML file → CLI
  flags. Each layer overrides the previous.
- Human-readable and human-editable.
- Supported by small, portable, C-compatible libraries available via vcpkg.
- Expressive enough to support nested or grouped parameters.
- Version-controllable without binary noise.

### 2.2 Decisions

| Concern | Choice | Status |
|---|---|---|
| Configuration file format | **TOML** via `tomlc99` | Decided |
| CLI parsing library | **argtable3** | Decided |
| Parameter model | Shared table in `core` with unified naming | Decided |

**TOML via `tomlc99`.** TOML is more expressive than INI (typed values,
arrays, grouped tables), still human-readable and diff-friendly, and
`tomlc99` is a minimal pure-C library. The familiarity argument for INI is
weak: the original format was not standard INI and required custom parsing
anyway.

**argtable3.** Portable (Linux, Windows, macOS), typed (int, double, string,
bool), available in vcpkg. Its declarative model — each argument is declared
as a typed struct field — aligns naturally with the shared parameter table
described in Section 2.4.

Both libraries are added to `vcpkg.json`.

### 2.3 CLI flags

The flags below are foreseen; they are provided as examples. The definitive
list will be documented in the usage documentation and will grow as
simulation parameters are implemented.

#### Flags common to both `sim-gpu` and `sim-stepper`

| Flag | Type | Default | Description |
|---|---|---|---|
| `--config <path>` | string | `biosim4-gpu.toml` | Path to the TOML parameter file |
| `--snapshot-in <path>` | string | _(none)_ | Resume from a saved population snapshot |
| `--snapshot-out <path>` | string | _(none)_ | Write a snapshot at the end of the run |
| `--generations <n>` | int | 1 | Number of generations to simulate |

Any simulation parameter defined in the shared parameter table (Section 2.4)
is also accepted as a CLI flag with the same name as its TOML key:
`--population 5000` overrides `population = 3000` in the config file.

#### `sim-gpu`-only flags

| Flag | Type | Default | Description |
|---|---|---|---|
| `--device <type>` | string | `any` | OpenCL device selection: `cpu` → `CL_DEVICE_TYPE_CPU` (PoCL on CI/ARM), `gpu` → `CL_DEVICE_TYPE_GPU`, `any` → `CL_DEVICE_TYPE_DEFAULT` |
| `--kernel-path <dir>` | string | _(none)_ | Directory to search for `.cl` overrides before falling back to embedded kernels |

`sim-stepper` does not expose `--device` or `--kernel-path`; it always runs
on the host CPU with no OpenCL dependency.

### 2.4 Shared parameter table and three-pass resolution

#### The parameter table

A single table in `core` defines every simulation parameter — its name, its
type, and its compiled-in default value:

```c
typedef enum {
    PARAM_INT, PARAM_FLOAT, PARAM_BOOL, PARAM_STRING
} biosim_param_type_t;

typedef struct {
    const char*         name;   // identical in TOML and on the CLI
    biosim_param_type_t type;
    union {
        int         i;
        double      f;
        bool        b;
        const char* s;
    } value;
} biosim_param_entry_t;
```

The `name` field is the single source of truth for the parameter's identity.
It is the same string in the TOML key, the CLI flag (`--<name>`), and any
future introspection mechanism. This naming convention is what makes the
integration transparent — no mapping table, no translation layer.

The parameter table struct, its type enum, and its compiled-in defaults live
in `core`. The TOML loader and the CLI parser live in each executable's
`main.c` — `core` knows neither `tomlc99` nor `argtable3`.

#### Three-pass resolution

Resolution happens at startup in the executable's `main.c`, in strict order:

```
Pass 1 — Compiled-in defaults    (the table as declared in core)
Pass 2 — TOML file               (overrides defaults)
Pass 3 — CLI flags               (overrides TOML)
```

Each pass iterates the same parameter table and overwrites the `value` union
for any parameter it finds. After the three passes, the table is frozen and
handed to the simulation as a read-only pointer — the same pattern as the
original biosim4's `ParamManager::getParamRef()`.

A parameter absent from both the CLI and the TOML file silently keeps its
compiled-in default. An unknown key in the TOML file or an unknown flag on
the CLI produces a warning (not an error), to ease forward compatibility
when new parameters are added.

## 3. Snapshot Binary Format

**Status: open design decision.** The format must be specified before any
snapshot read/write code is written, because it is generated by `sim-gpu`,
and consumed by `sim-stepper`.

### 3.1 Context

A snapshot is a point-in-time serialization of a generation: all surviving
agents' genomes must be saved, as well as the related simulation parameters.
Snapshots are:

- Written by `sim-gpu` at the end of each generation (or on demand).
- Read by `sim-gpu` to resume a run.
- Read by `sim-stepper` to replay a generation step by step.

They are binary (not text) because agent genome counts can reach hundreds of
thousands and a text encoding would be impractically large.
