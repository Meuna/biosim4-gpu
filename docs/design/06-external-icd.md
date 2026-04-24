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
