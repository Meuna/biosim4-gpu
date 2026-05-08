# Code Conventions

## Naming

- **Files and directories**: `snake_case.c`, `snake_case.h`. Package
  directory names may use hyphens (`sim-gpu`, `sim-stepper`) — they
  are not C identifiers.
- **Public API symbols**: `biosim_` prefix — e.g. `biosim_genome_mutate`,
  not `mutate_genome`.
- **Types**: `biosim_*_t` suffix — e.g. `biosim_coord_t`, `biosim_gene_t`.
- **OpenCL kernel entry points**: `k_` prefix — e.g. `k_feedforward`,
  `k_movement`. No other scope prefix (`s_`, `g_`, etc.) anywhere in
  the codebase.

## Section separators

`.c` files with multiple logical groups must separate them with a titled
79-character banner using U+2500 (`─`):

```c
/* ── lifecycle ──────────────────────────────────────────────────────────── */
```

## Error handling

- Functions that can fail return `biosim_status_t`.
- `assert` is permitted only for invariants that indicate a programming
  bug — not for recoverable runtime conditions.
- `biosim_strerror(code)` maps any `biosim_status_t` to a human-readable
  string; use it in error messages instead of printing the raw integer.

## Alloc/goto/free discipline

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

## Error logging discipline

- Functions that exclusively return `BIOSIM_ERR_NOMEM` do **not** log
  — allocation failures are reported by the first caller that has
  broader context.
- All other functions log once at their `exit:` label when
  `returncode != BIOSIM_OK`, using
  `BIOSIM_ERRORF("... (%s)", biosim_strerror(returncode))`.
- Callers do not re-log errors returned from callees.

## Host/device portability

Three headers are shared with OpenCL kernel sources: `core/types.h`,
`core/rng.h`, `core/gene.h`. They must compile as both C11 and OpenCL C:

- No `<stdio.h>`, `<stdlib.h>`, `<string.h>`, or any other host-only
  standard header.
- No heap allocation, no function pointers, no host struct types with
  pointer members.
- Fixed-width integer types defined via the portability guard in `types.h`:

  ```c
  #ifdef __OPENCL_VERSION__
  typedef short  int16_t;
  typedef ushort uint16_t;
  #else
  #include <stdint.h>
  #endif
  ```

Every such header carries a prologue comment: `/* OPENCL-SAFE: ... */`.
Headers that are host-only carry
`/* HOST-ONLY: do not include from .cl files. */`.

## No mutable global state in `core`

All `core` functions take their state by parameter. No file-scope mutable
variables, no singletons, no thread-local pseudo-globals. `core` is called
from two execution contexts (single-threaded stepper and future GPU host
thread) and must behave identically in both.

## CMake

Use target-first commands only: `target_include_directories`,
`target_link_libraries`, `target_compile_definitions`. No
`include_directories()` or `add_definitions()` at the top level.

## Testing

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
