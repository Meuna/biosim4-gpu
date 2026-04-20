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

The simulation is driven by a collection of parameters that configure
everything defining a simulation: the grid size, the population size, the
genome length, mutation rate, challenge types, and dozens of other knobs.
biosim4 originally used a custom INI-style format parsed by hand.

The GPU port replaces it with a design that is:

- **Multi-source with precedence:** compiled-in defaults → TOML file → CLI
  flags. Each layer overrides the previous.
- **Table-driven:** a single parameter table in `core` is the source of truth
  for what parameters exist, their types, their names, and their defaults.
  CLI flags and TOML keys are derived from this table, not declared
  separately.
- Human-readable and human-editable.
- Supported by small, portable, C-compatible libraries available via vcpkg
  or vendored.

### 2.2 Decisions

| Concern | Choice | Status |
|---|---|---|
| Configuration file format | **TOML** via `tomlc17` | Decided |
| CLI parsing library | **argtable3** | Decided |
| Parameter model | Shared table in `core` with unified naming | Decided |

**TOML via `tomlc17`.** TOML is more expressive than INI (typed values,
arrays, grouped tables), still human-readable and diff-friendly, and
`tomlc17` is a minimal pure-C library. The familiarity argument for INI is
weak: the original format was not standard INI and required custom parsing
anyway. `tomlc17` is vendored in `third_party/tomlc17/` (two files, no
upstream CMake support).

**argtable3.** Portable (Linux, Windows, macOS), typed (int, double, string,
bool), available in vcpkg. Its declarative model — each argument is declared
as a typed struct field — aligns naturally with the shared parameter table.

### 2.3 Parameter Table — the core of the system

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
    const char*           name;          // identical in TOML key and CLI flag
    biosim_param_type_t   type;
    biosim_param_value_t  default_value; // compiled-in default
    biosim_param_value_t  value;         // resolved value after three-pass
    bool                  is_set;        // true if overridden by TOML or CLI
} biosim_param_entry_t;
```

The `name` field is the **single identifier** for the parameter everywhere:
TOML key, CLI flag (`--<name>`), and introspection. This naming convention is
what makes the integration transparent — no mapping table, no translation
layer.

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

The container is a dynamically-sized array of entries. `core` provides
the base entries; each simulator appends its own before resolution begins.

#### 2.3.3 Core API

All functions below live in `core`. None of them knows about tomlc17 or
argtable3 — they operate on native C types only.

**Lifecycle:**

```c
// Initialize with compiled-in simulation defaults
biosim_status_t biosim_params_init(biosim_params_t* p);

// Add simulator-specific entries (called before resolution)
biosim_status_t biosim_params_extend(biosim_params_t* p,
                                     const biosim_param_entry_t* extras,
                                     size_t count);

// Release resources
void biosim_params_free(biosim_params_t* p);
```

`biosim_params_init` populates the table with every simulation parameter
that is common to both simulators: population size, grid dimensions, genome
length, mutation rate, steps per generation, challenge type, etc. The full
list is implementation-dependent and will grow as features are ported.

`biosim_params_extend` appends simulator-specific entries. For `sim-gpu`
this includes `device` and `kernel-path`; for `sim-stepper` this may include
trace-related options. Extension happens once, before the three-pass
resolution.

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
Pass 1 — biosim_params_init() + biosim_params_extend()
          → table is populated with compiled-in defaults

Pass 2 — TOML glue iterates the TOML file, calls biosim_params_set_*()
          → overrides defaults for any key present in the file

Pass 3 — CLI glue iterates parsed arguments, calls biosim_params_set_*()
          → overrides TOML (and defaults) for any flag on the command line
```

After the three passes, the table is frozen and handed to the simulation
as a `const biosim_params_t*` — the same pattern as the original biosim4's
`ParamManager::getParamRef()`.

### 2.5 Table-driven CLI generation

Each simulator generates its argtable3 declarations by iterating the
parameter table, not by hardcoding flags:

```c
// in sim-gpu/main.c (or sim-stepper/main.c)
void** argtable = calloc(biosim_params_count(&params) + EXTRA, sizeof(void*));
size_t n = 0;

for (size_t i = 0; i < biosim_params_count(&params); i++) {
    const biosim_param_entry_t* e = biosim_params_entry(&params, i);
    switch (e->type) {
        case PARAM_INT:
            argtable[n++] = arg_intn(NULL, e->name, "<n>", 0, 1, "");
            break;
        case PARAM_FLOAT:
            argtable[n++] = arg_dbln(NULL, e->name, "<v>", 0, 1, "");
            break;
        case PARAM_BOOL:
            argtable[n++] = arg_litn(NULL, e->name, 0, 1, "");
            break;
        case PARAM_STRING:
            argtable[n++] = arg_strn(NULL, e->name, "<s>", 0, 1, "");
            break;
    }
}
// add non-param flags: --help, --version, --config
argtable[n++] = arg_file0(NULL, "config", "<path>", "TOML config file");
argtable[n++] = arg_lit0("h", "help", "print help");
argtable[n++] = arg_end(20);
```

**Consequence: adding a simulation parameter = adding one entry in `core`'s
init function.** The CLI and TOML parsing pick it up automatically with no
changes to `sim-gpu` or `sim-stepper`.

### 2.6 Table-driven TOML loading

Same principle — the TOML glue iterates the parameter table, not a hardcoded
list of keys:

```c
// in sim-gpu/main.c (or sim-stepper/main.c)
for (size_t i = 0; i < biosim_params_count(&params); i++) {
    const biosim_param_entry_t* e = biosim_params_entry(&params, i);
    toml_datum_t d;
    switch (e->type) {
        case PARAM_INT:
            d = toml_int_in(conf, e->name);
            if (d.ok) biosim_params_set_int(&params, e->name, (int)d.u.i);
            break;
        case PARAM_FLOAT:
            d = toml_double_in(conf, e->name);
            if (d.ok) biosim_params_set_float(&params, e->name, d.u.d);
            break;
        case PARAM_BOOL:
            d = toml_bool_in(conf, e->name);
            if (d.ok) biosim_params_set_bool(&params, e->name, d.u.b);
            break;
        case PARAM_STRING:
            d = toml_string_in(conf, e->name);
            if (d.ok) {
                biosim_params_set_string(&params, e->name, d.u.s);
                free(d.u.s);  // tomlc17 allocates strings
            }
            break;
    }
}
```

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
