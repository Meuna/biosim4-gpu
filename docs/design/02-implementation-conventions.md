# Implementation Conventions

**Purpose:** List the structural invariants that govern how code is laid out
inside every package. These rules are stable across the life of the project
and apply to every contributor. Where possible, each rule is enforced by the
build system or CI rather than by prose alone. Build-level conventions (modern
CMake target-first discipline) are documented separately in
[`03-portable-build.md`](03-portable-build.md).

## Table of Contents

1. [Package Boundaries and Dependency Direction](#1-package-boundaries-and-dependency-direction)
2. [Public vs. Private Headers](#2-public-vs-private-headers)
3. [Host/Device Portability of Shared Modules](#3-hostdevice-portability-of-shared-modules)
4. [No Mutable Global State in `core`](#4-no-mutable-global-state-in-core)
5. [Tests Mirror Sources](#5-tests-mirror-sources)
6. [Naming Conventions](#6-naming-conventions)
7. [File Length and Cohesion](#7-file-length-and-cohesion)
8. [Error Handling](#8-error-handling)
9. [Scope — What This Document Does Not Prescribe](#9-scope--what-this-document-does-not-prescribe)

## 1. Package Boundaries and Dependency Direction

- `core` depends only on the C standard library.
- `sim-gpu` depends on `core` + OpenCL.
- `sim-stepper` depends on `core` only.
- `viz` (future) depends on `core` and on a rendering library.
- Reverse dependencies are forbidden. `core` must never `#include` anything
  from `sim-gpu`, `sim-stepper`, or `viz`.

**Enforcement.** The CMake `target_link_libraries` graph expresses this
directly. A violation fails the link step. A lightweight CI check can also
`grep` for disallowed includes to give a clearer error message earlier.

## 2. Public vs. Private Headers

- **Public headers** live under `include/biosim/<package>/`. They form the
  API consumed by other packages and by tests.
- **Private headers** live alongside their implementation in `src/`. They
  are never visible outside the package.
- A header in `include/` must compile standalone (every symbol it uses is
  either declared locally or included explicitly).

## 3. Host/Device Portability of Shared Modules

- Modules whose symbols are called from OpenCL kernels (the POD types and
  the RNG, plus any future sharing) must compile both as C11 host code and
  as OpenCL C.
- This means: no `<stdio.h>`, no `<stdlib.h>`, no `<string.h>`, no host-only
  macros inside those headers. Only fixed-width integer types, arithmetic,
  and inline functions.
- Every concerned header carries a prologue comment flagging the constraint
  so future edits don't silently break OpenCL compilation.

**Implemented example — `packages/core/include/biosim/core/types.h`:**

OpenCL C has no `<stdint.h>`, so `int16_t` / `uint16_t` are undefined there.
Its scalar integer equivalents are `short` and `ushort`. The portability guard
pattern resolves this:

```c
#ifdef __OPENCL_VERSION__   /* GPU kernel compilation */
typedef short  int16_t;
typedef ushort uint16_t;
#else                       /* C11 host compilation */
#include <stdint.h>
#endif
```

Headers that are host-only (e.g. any struct with a heap pointer) must not use
this guard — they are simply not included by kernel sources. Such headers
carry the complementary note: `HOST-ONLY: do not include from .cl files.`

See [`03-portable-build.md`](03-portable-build.md) for the language-level
statement of this rule.

## 4. No Mutable Global State in `core`

- `core` functions take their state by parameter. No file-scope mutable
  variables, no singletons, no thread-local pseudo-globals.
- Rationale: `core` is called from two different execution contexts (GPU
  host thread, single-threaded stepper) and may later be called from a
  third (GPU kernel compilation). Hidden global state would silently make
  the two simulators behave differently.

## 5. Tests Mirror Sources

- When a source module `foo` in a package's `src/` has non-trivial logic, a
  corresponding `test_foo.c` lives in the package's `tests/`.
- Tests that span modules (integration-style) are named by intent, not by
  source file (e.g. `test_cross_simulator_equivalence.c`).
- The mirror rule is a soft convention, not a build-enforced rule — it
  applies where it aids navigation, not dogmatically.

## 6. Naming Conventions

- **Files and directories:** `snake_case.c`, `snake_case.h`. No hyphens in C
  source file names (they confuse some build tools and debuggers). Package
  directory names can use hyphens (`sim-gpu`, `sim-stepper`) because they
  are not C identifiers.
- **Public API symbols:** prefixed with `biosim_` to avoid collisions in
  consumer code. Example: `biosim_genome_mutate`, not `mutate_genome`.
- **Types:** the same prefix, `biosim_coord_t`, `biosim_gene_t`, etc., with
  a `_t` suffix for typedef'd aggregates.
- **OpenCL kernel entry points:** prefixed `k_` to visually distinguish them
  from host functions in logs and diagnostics. Example: `k_feedforward`,
  `k_movement`.
- **scope prefix:** only prefix OpenCL kernel entry pmoint with `k_` to
  visually distinguish them from host functions in logs and diagnostics.
  Example: `k_feedforward`, `k_movement`. Do not use any other scope
  prefix (s_, g_ etc.). Rely on IDE scope resolution and syntax highlighting.

## 7. File Length and Cohesion

- No hard line limit. A file should contain exactly one cohesive module's
  worth of code. If a `.c` file drifts past ~800 lines or accumulates
  unrelated responsibilities, it is a candidate for splitting.
- Splitting is an implementation decision made when the evidence is in the
  code, not preemptively.

## 8. Error Handling

- All functions that can fail return a status code (`biosim_status_t` or an
  equivalent enum in `core`). No asserts as a substitute for error handling.
- Asserts are permitted for invariants that would indicate a bug
  (`assert(alive[i] == 0 || alive[i] == 1)`), not for recoverable runtime
  conditions.

## 9. Scope — What This Document Does Not Prescribe

- Specific `.c` / `.h` file names inside each package. That is an
  implementation decision made at coding time, guided by the module roles
  described in [`01-repository-structure.md`](01-repository-structure.md)
  Sections 5, 6, and 7.
- The exact content of any given file.
- The order of functions within a file.
- Comment style beyond the portability-prologue rule of Section 3.
