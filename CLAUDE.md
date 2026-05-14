# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project

GPU port of [biosim4](https://github.com/davidrmiller/biosim4) (by David R.
Miller) using OpenCL. Four packages are implemented: `core`, `cfgparse`,
`sim-stepper`, and `sim-gpu`.

## Working with this repository

- **Keep the changelog up-to-date.**

- **Keep the documentation up-to-date.** When implementing a feature, update the
  documentation so it describes the system as it exists after your change. Remove
  any outdated statements.

- **Each new source module needs a test module.** When adding
  `packages/core/src/foo.c`, also add `packages/core/tests/test_foo.c`
  and register it in `packages/core/tests/CMakeLists.txt`. Use the
  Unity framework (see `docs/conventions.md`).

- **Increment the snapshot schema version** when modifying `biosim_sensor_t` or
  `biosim_action_t` in `io_defs.h`.

- **Increment the snapshot format version** when modifying the snapshot interface.

- **Conventions are normative.** The rules in `docs/conventions.md` (naming,
  error handling, host/device portability, no global state in `core`) apply to
  every file written in this repository.

  - **Code Quality.** Every time you edit or create files, you MUST
    complete this sequence before considering the task done:
    1. `cmake --build --preset debug` — must compile with zero errors
    2. `ctest --preset debug` — all tests must pass
    3. `cmake --build --preset debug --target lint` — fix every error
       reported EXCEPT readability-function-cognitive-complexity;
       repeat until the output is clean
    4. `cmake --build --preset debug --target format` — apply formatting
    5. `cmake --build --preset debug` — re-compile to confirm formatting
       did not break anything
    6. `ctest --preset debug` — re-run tests to confirm formatting did not
       break anything

    A task is NOT complete while lint reports any error or warning.

  - **readability-function-cognitive-complexity special case.** When facing
    a cognitive-complexity error, do not fix blindly. Instead, ask the user
    to decide whether to fix or add a NOLINTNEXTLINE flag.

## Build System

The build uses **CMake + vcpkg** (`VCPKG_ROOT` must be set, see `docs/build.md`):

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
cmake --build --preset debug --target format
cmake --build --preset debug --target lint
```

## Portability Pitfalls

Three mistakes are easy to make and hard to spot. They break the build on
Windows or on OpenCL but compile cleanly on Linux host:

1. **Host-only includes in shared headers.** Headers consumed by OpenCL kernels
   (`core/grid_defs.h`, `core/io_defs.h`, `core/challenge_defs.h`,
   `core/rng.h`, `core/gene.h`) must not include `<stdio.h>`,
   `<stdlib.h>`, `<string.h>`, or any other host-only standard header. Every
   such header carries a prologue comment flagging the constraint — respect it.

2. **Non-portable compiler flags.** `-Wall`, `-Wextra`, `-fsanitize=address`
   are GCC/Clang only. MSVC uses `/W4`, `/permissive-`, `/fsanitize=address`.
   Never add flags directly to a `CMakeLists.txt`; add them to
   `cmake/CompilerWarnings.cmake` or `cmake/Sanitizers.cmake` where the
   branching on `CMAKE_C_COMPILER_ID` already happens.

3. **`int` where `uint16_t` is required.** Grid cell indices and gene counts
   must use the exact fixed-width types; silent sign-extension or narrowing can
   corrupt GPU kernel results.

4. **Breaking the Windows build.** The repository must build on both
   Linux (GCC/Clang) and Windows (MSVC). Common failure modes: using
   POSIX-only headers (`<unistd.h>`, `<sys/types.h>`) in files that are
   not platform-gated; GCC/Clang pragmas or `__attribute__` without an
   MSVC guard; compiler flags added directly to `CMakeLists.txt` instead
   of `cmake/CompilerWarnings.cmake`. See pitfall 2 for flag portability.

## Architecture

Four packages, strict acyclic dependency graph:

```
core (static lib, libc only)
  └── cfgparse (static lib — CLI/TOML parsing)
      ├── sim-stepper (executable — single-threaded CPU reference)
      └── sim-gpu     (static lib + executable — OpenCL GPU simulator)
```

**`core`** — all shared simulation logic: genome operators, neural network
compilation, sensor/action catalogues, challenge evaluation, portable xorshift64
RNG, snapshot serialization, parameter data model (`biosim_params_t`).
`biosim_sim_t` is the complete simulation state; call `biosim_sim_create` with
a parsed `biosim_params_t` and `biosim_challenge_spec_t` to configure all
fields, allocate heap resources, and spawn the initial population.

**`cfgparse`** — CLI argument parsing and TOML config loading. Each simulator's
`main.c` defines its own exhaustive entry table and calls `biosim_params_parse`.
No shared defaults; no extension mechanism. The parameter data model lives in
`core/params.h`.

**`sim-stepper`** — orchestration loop only. All simulation logic lives in
`core`; the stepper adds the parameter table and the generation/step loop.

**`sim-gpu`** — OpenCL GPU simulator. `sim-gpu-lib` provides the kernel registry
(two-level lookup: filesystem override → embedded fallback) and the OpenCL runner
(platform/device/context/queue lifecycle). The `biosim-gpu` executable wires
parameters to a simulation and dispatches kernels.

## Code Conventions (see `docs/conventions.md`)

- **Files:** `snake_case.c`, `snake_case.h`
- **Section separators:** `.c` files with multiple logical groups must separate
  them with a titled 79-char banner using U+2500 (`─`)
- **Public API:** `biosim_` prefix — e.g. `biosim_genome_mutate`
- **Types:** `biosim_*_t` — e.g. `biosim_coord_t`
- **OpenCL kernel entry points:** `k_` prefix only — no other scope prefix
- **No mutable global state** in `core` — all state passed by parameter
- **Error handling:** functions return `biosim_status_t`; asserts only for
  invariants that indicate bugs
- **Host/device portability:** `core/grid_defs.h`, `core/io_defs.h`,
  `core/challenge_defs.h`, `core/rng.h`, `core/gene.h` must compile as
  both C11 and OpenCL C — no `<stdio.h>`, `<stdlib.h>`, `<string.h>`.
  Use `_defs.h` suffix for pure type/enum/constant-only shared headers.
  Mark all shared headers with `/* HOST/DEVICE: ... */` prologue comment.
- **CMake:** target-first only (`target_*` commands); no top-level
  `include_directories()` or `add_definitions()`

## Alloc/goto/free discipline

Functions that allocate multiple resources follow this pattern:

1. `memset(out, 0, sizeof(*out))` — zero the struct before any allocation so
   that free can safely call `free(NULL)` on uninitialized pointers.
2. Sequential allocations, each guarded by `goto exit` on failure.
3. Single `exit:` label: if `returncode != BIOSIM_OK`, call the free function
   (NULL-safe) then log the error.
4. Free functions tolerate NULL on every member and NULL on the struct itself.

## Error logging discipline

- Functions that exclusively return `BIOSIM_ERR_NOMEM` do **not** log. Callers
  with broader context are responsible.
- All other functions log once at their `exit:` label on failure:
  `BIOSIM_ERRORF("... (%s)", biosim_strerror(returncode))`.
- Callers do not re-log errors they receive from callees.

## Key Files

- `docs/architecture.md` — package structure, key types and functions
- `docs/conventions.md` — code layout invariants
- `docs/build.md` — CMake/vcpkg setup, build targets
- `docs/gpu-design.md` — planned GPU data model and kernel pipeline
- `docs/formats.md` — snapshot binary format, TOML parameter format
- `docs/usage.md` — CLI reference, challenges, barriers

## Build Files

Do not read from `build/` — it is a derived artifact and may not exist.

## Third Party Files

Vendored libraries in `third_party/` are read-only, except when instructed to
bump the version.
