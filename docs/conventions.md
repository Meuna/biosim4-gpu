# Code Conventions

## C / OpenCL

Applies to all C/H/CL files: packages `core`, `cfgparse`, `sim-ref`,
`sim-gpu`, and `sim-wasm`.

### Naming

- **Files and directories**: `snake_case.c`, `snake_case.h`. Package
  directory names may use hyphens (`sim-gpu`, `sim-ref`) — they
  are not C identifiers.
- **Public API symbols**: `biosim_` prefix — e.g. `biosim_genome_mutate`,
  not `mutate_genome`.
- **Types**: `biosim_*_t` suffix — e.g. `biosim_coord_t`, `biosim_gene_t`.
- **OpenCL kernel entry points**: `k_` prefix — e.g. `k_feedforward`,
  `k_movement`. No other scope prefix (`s_`, `g_`, etc.) anywhere in
  the codebase.

### Section separators

`.c` files with multiple logical groups must separate them with a titled
79-character banner using U+2500 (`─`):

```c
/* ── lifecycle ──────────────────────────────────────────────────────────── */
```

### Error handling

- Functions that can fail return `biosim_status_t`.
- `assert` is permitted only for invariants that indicate a programming
  bug — not for recoverable runtime conditions.
- `biosim_strerror(code)` maps any `biosim_status_t` to a human-readable
  string; use it in error messages instead of printing the raw integer.

### Alloc/goto/free discipline

Functions that allocate multiple resources follow this pattern:

1. `memset(out, 0, sizeof(*out))` — zero the output struct before any
   allocation, so that the free function can safely call `free(NULL)`
   on any uninitialized pointer.
2. Sequential allocations, each guarded by
   `if (ptr == NULL) { returncode = BIOSIM_ERR_NOMEM; goto exit; }`.
3. A single `exit:` label at the end. If `returncode != BIOSIM_OK`,
   call the matching free function (which handles partial initialization
   via NULL checks), then log the error.
4. Free functions must tolerate NULL on every member and NULL on the
   struct itself.

### Error logging discipline

- Functions that exclusively return `BIOSIM_ERR_NOMEM` do **not** log
  — allocation failures are reported by the first caller that has
  broader context.
- All other functions log once at their `exit:` label when
  `returncode != BIOSIM_OK`, using
  `BIOSIM_ERRORF("... (%s)", biosim_strerror(returncode))`.
- Callers do not re-log errors returned from `biosim_*` callees.
- Callers exhaustively log OpenCL errors returned from `cl*` callees,
  using `BIOSIM_ERRORF("cl... failed (OpenCL %d)", (int)err)` and
  return `BIOSIM_ERR_OPENCL`. Macros in `cl_macros.h` help reduce
  boilerplate.

### Host/device portability

Five headers are shared with OpenCL kernel sources and prepended to every
kernel program as a preamble: `core/grid_defs.h`, `core/rng.h`,
`core/gene.h`, `core/io_defs.h`, `core/challenge_defs.h`.

#### Naming convention

- **`*_defs.h`** — device-portable header containing only enum/type/macro
  definitions (no functions, no stdlib, no host struct types with pointers).
  These are the portable vocabulary headers — safe to include from both C11
  host code and OpenCL C kernel sources.
- **`*.h`** (no `_defs` suffix) — host-only header. Marked with
  `/* HOST-ONLY: ... */`. May use `<stdbool.h>`, heap pointers, function
  signatures that reference `biosim_sim_t`.

Headers with inline functions that are also device-portable (`rng.h`,
`gene.h`) follow their own domain naming — the `_defs` suffix specifically
means "type/enum/constant definitions only, no functions."

#### Portability requirements

Shared headers must compile as both C11 and OpenCL C:

- No `<stdio.h>`, `<stdlib.h>`, `<string.h>`, or any other host-only
  standard header.
- No heap allocation, no function pointers, no host struct types with
  pointer members.
- Fixed-width integer types defined via the portability guard in
  `grid_defs.h`:

  ```c
  #ifdef __OPENCL_VERSION__
  typedef short  int16_t;
  typedef ushort uint16_t;
  typedef int    int32_t;
  typedef uint   uint32_t;
  #else
  #include <stdint.h>
  #endif
  ```

Every `_defs.h` header carries a prologue comment:
`/* HOST/DEVICE: this header is included by OpenCL kernel sources. ... */`.
Host-only headers carry `/* HOST-ONLY: ... */`.

### No mutable global state in `core`

All `core` functions take their state by parameter. No file-scope mutable
variables, no singletons, no thread-local pseudo-globals. `core` is called
from two execution contexts (single-threaded stepper and future GPU host
thread) and must behave identically in both.

### CMake

Use target-first commands only: `target_include_directories`,
`target_link_libraries`, `target_compile_definitions`. No
`include_directories()` or `add_definitions()` at the top level.

### Testing

Each source module has a mirror test file. The convention:

- `packages/core/src/foo.c` → `packages/core/tests/test_foo.c`
- `packages/params/src/bar.c` → `packages/params/tests/test_bar.c`

Test files use the [Unity](https://github.com/ThrowTheSwitch/Unity) C test
framework. Each new test executable must be registered in the package's
`tests/CMakeLists.txt` and linked against `unity` and the package under
test.

Shared test helpers live in
`packages/core/tests/sim_test_utils.{c,h}` — add to this file when
multiple test modules need the same setup logic.

## TypeScript / Svelte

Applies to `packages/webapp/`.

### Naming

- **Modules**: `camelCase.ts`
- **Svelte components**: `PascalCase.svelte`

### Module organisation

Avoid barrel files (`index.ts` re-exports) unless a genuine package
boundary requires one.

### Testing

Each `src/lib/foo.ts` module has a mirror test file `src/lib/foo.test.ts`
using [Vitest](https://vitest.dev/). Component tests use
`@testing-library/svelte`.

```sh
bun run --cwd packages/webapp test          # non-interactive
bun run --cwd packages/webapp test:watch    # watch mode
```

### Linting and formatting

- **ESLint 9** flat config: `packages/webapp/eslint.config.js`. Run via
  `cmake --build --preset webapp --target lint`.
- **Prettier** with `prettier-plugin-svelte`. Run via
  `cmake --build --preset webapp --target format`.

## Commit messages

### Format

```
gh-{N}: {imperative description}
```

where `{N}` is the GitHub issue number. Examples:

- `gh-51: add PR workflow section to CLAUDE.md`
- `gh-48: design kinematic and grid canvas feature`

### Rules

- Use the imperative mood: "add", not "added" or "adds".
- No period at the end of the description.
- Every commit must reference an issue number.
- Multiple commits per PR are allowed; each references the same issue number.
- Do **not** put `closes` in commit messages — use `Closes #{N}` in the PR
  body instead so that GitHub closes the issue on merge.

### Review-response commits

When addressing PR review comments, each commit addresses one comment
atomically. Use the same format:

```
gh-{N}: {description of what changed in response to review}
```
