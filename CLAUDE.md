# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project

GPU port of [biosim4](https://github.com/davidrmiller/biosim4) (by David R.
Miller) using OpenCL. Six packages are implemented: `core`, `cfgparse`,
`sim-ref`, `sim-gpu`, `sim-wasm`, and `webapp`.

## Two build branches

This repo has two independent build trees. Every task belongs to one or both.
**Identify your branch before starting work.**

| Branch | Packages | CMake presets | When it applies |
|---|---|---|---|
| **Native** | `core`, `cfgparse`, `sim-ref`, `sim-gpu` | `debug` `release` `asan` `ci` | Any C/H/CL file outside `packages/sim-wasm/` and `packages/webapp/` |
| **Webapp** | `sim-wasm`, `webapp` | `webapp` | Any file under `packages/sim-wasm/` or `packages/webapp/` |

A change touching files from both branches requires the quality sequence to be
run on **both** branches.

> `sim-wasm` is C code, but its lint target lives in the webapp preset — treat
> it as the webapp branch for all quality-check purposes.

### Quality check sequence

The sequence is the same for both branches:

1. **build** — must compile with zero errors
2. **test** — all tests must pass
3. **lint** — fix every error/warning (see cognitive-complexity note below)
4. **format** — apply formatting
5. **build** — confirm format did not break compilation
6. **test** — confirm format did not break tests

A task is **NOT** complete while lint reports any error or warning.

Branch-specific commands:

| Step | Native (`--preset debug`) | Webapp (`--preset webapp`) |
|---|---|---|
| build | `cmake --build --preset debug` | `cmake --build --preset webapp` |
| test | `ctest --preset debug` | `bun run --cwd packages/webapp test` |
| lint | `cmake --build --preset debug --target lint` | `cmake --build --preset webapp --target lint` |
| format | `cmake --build --preset debug --target format` | `cmake --build --preset webapp --target format` |

### `readability-function-cognitive-complexity` special case

When facing a cognitive-complexity lint error, do not fix blindly. Ask the user
to decide whether to refactor or add a `NOLINTNEXTLINE` flag.

## Working with this repository

- **Keep the changelog up-to-date.**

- **Keep the documentation up-to-date.** When implementing a feature, update the
  documentation so it describes the system as it exists after your change. Remove
  any outdated statements.

- **Each new source module needs a test module.**
  - _Native branch:_ when adding `packages/core/src/foo.c`, also add
    `packages/core/tests/test_foo.c` and register it in
    `packages/core/tests/CMakeLists.txt`. Use the Unity framework
    (see `docs/conventions.md`).
  - _Webapp branch:_ when adding `packages/webapp/src/lib/foo.ts`, also add
    `packages/webapp/src/lib/foo.test.ts`. Use Vitest; component tests use
    `@testing-library/svelte`.

- **Increment the snapshot schema version** when modifying `biosim_sensor_t` or
  `biosim_action_t` in `io_defs.h`.

- **Increment the snapshot format version** when modifying the snapshot interface.

- **Conventions are normative.** The rules in `docs/conventions.md` apply to
  every file written in this repository.

## Browser Testing (webapp branch)

### Setup — install chrome-devtools-mcp

```sh
claude mcp add chrome-devtools --scope user bunx chrome-devtools-mcp@latest
```

Restart Claude Code after adding the server so it is available in the session.

### Webapp dev check

After completing the quality check sequence for a webapp change, verify the
app works correctly in a real browser: run
`cmake --build --preset webapp --target dev`, then load http://localhost:5173
via chrome-devtools-mcp and confirm zero console errors and zero warnings.

A webapp task is **NOT** complete if the browser console shows any error or
warning introduced by your change.

Terminate the dev server and mcp tab when you are done.

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

Six packages across two build trees:

**Native tree** (CMake presets: `debug`, `release`, `asan`, `ci`):
```
core (static lib, libc only)
  └── cfgparse (static lib — CLI/TOML parsing)
      ├── sim-ref (executable — single-threaded CPU reference)
      └── sim-gpu (static lib + executable — OpenCL GPU simulator)
```

**Webapp tree** (CMake presets: `wasm`, `webapp`; requires Emscripten + Bun):
```
core (static lib, compiled to WASM)
  └── sim-wasm (Emscripten ES6 module — WASM bindings)
        └── webapp (Svelte SPA — loads sim-wasm in a Web Worker)
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

**`sim-ref`** — deterministic reference scheduler. All simulation logic lives in
`core`; `sim-ref` adds the parameter table and the generation/step loop.

**`sim-gpu`** — OpenCL GPU simulator. `sim-gpu-lib` provides the kernel registry
(two-level lookup: filesystem override → embedded fallback) and the OpenCL runner
(platform/device/context/queue lifecycle). The `biosim-gpu` executable wires
parameters to a simulation and dispatches kernels.

**`sim-wasm`** — Emscripten WASM bindings (`packages/sim-wasm/src/bindings.c`).
Compiled to `biosim.mjs` + `biosim.wasm` (ES6 module). Consumed by the webapp
via a Web Worker.

**`webapp`** — Svelte 5 SPA (`packages/webapp/`). Loads `sim-wasm` inside a
Web Worker (`src/workers/sim.worker.ts`). Built with Vite 6. Tooling: ESLint
(flat config), Prettier, Vitest. See `docs/build.md` for the dev server and
build commands.

## Code Conventions (see `docs/conventions.md`)

### C / OpenCL (native branch)

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

### TypeScript / Svelte (webapp branch)

- **Files:** `camelCase.ts` for modules, `PascalCase.svelte` for components
- **No barrel files:** avoid `index.ts` re-exports unless a package boundary
  genuinely requires one
- **Testing:** `src/lib/foo.ts` → `src/lib/foo.test.ts` (Vitest); component
  tests use `@testing-library/svelte`
- **No global mutable state:** use Svelte stores or passed props; no
  module-level mutable variables
- **Lint/format:** run via the `webapp` preset (`--target lint`, `--target format`)

### Webapp Styling

- Style with scoped CSS in `.svelte` `<style>` blocks. Do not add Tailwind or any
  utility-class framework.
- Reference semantic token aliases from `src/styles/tokens.css`. Do not hardcode
  colors, spacings, or font names in component styles.
- To change the theme, edit `src/styles/tokens.css` only. Never retheme by editing
  component styles.
- Retheme the whole app by swapping `--_accent` (and the surface/text raw values
  if needed). Verify contrast remains high after any palette change.
- Two-tier CSS framework: element defaults belong in `src/styles/base.css`;
  shared, reusable component classes (`.button`, `.panel`, `.field-row`, etc.)
  belong in `src/styles/primitives.css`, imported from `app.css` after `base.css`.
  Only component-specific styles belong in a component's scoped `<style>` block.
  **Any style pattern used by 2 or more components MUST live in `primitives.css`**
  — never duplicated inline.

## Alloc/goto/free discipline (native branch)

Functions that allocate multiple resources follow this pattern:

1. `memset(out, 0, sizeof(*out))` — zero the struct before any allocation so
   that free can safely call `free(NULL)` on uninitialized pointers.
2. Sequential allocations, each guarded by `goto exit` on failure.
3. Single `exit:` label: if `returncode != BIOSIM_OK`, call the free function
   (NULL-safe) then log the error.
4. Free functions tolerate NULL on every member and NULL on the struct itself.

## Error logging discipline (native branch)

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

## Third Party Files

Vendored libraries in `third_party/` are read-only, except when instructed to
bump the version.
