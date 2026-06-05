# core

Static library. All shared simulation logic: genome operators, neural network
compilation, sensor/action catalogues, challenge evaluation, portable xorshift64
RNG, snapshot serialization, and the parameter data model (`biosim_params_t`).
`biosim_sim_t` is the complete simulation state; call `biosim_sim_create` with
a parsed `biosim_params_t` and `biosim_challenge_spec_t` to configure all
fields, allocate heap resources, and spawn the initial population.

**Build branch: Native.** No dependency beyond libc.

## Quality check sequence

Run every step in order for every task:

```sh
cmake --build --preset debug
ctest --preset debug
cmake --build --preset debug --target lint
cmake --build --preset debug --target format
cmake --build --preset debug
ctest --preset debug
```

A task is **NOT** complete while lint reports any error or warning.

## Naming

- **Files**: `snake_case.c`, `snake_case.h`
- **Public API**: `biosim_` prefix — e.g. `biosim_genome_mutate`
- **Types**: `biosim_*_t` — e.g. `biosim_coord_t`
- **No scope prefix** (`s_`, `g_`, etc.) on any symbol. Use IDE navigation, not prefixes.
- **Section separators**: `.c` files with multiple logical groups use a
  79-char banner: `/* ── lifecycle ──────────────────────────────────────────────────────────── */`

## Error handling

- Return `biosim_status_t` from all functions that can fail.
- `assert` only for programming invariants, never for runtime conditions.
- Use `biosim_strerror(code)` in error messages, not the raw integer.

## Alloc/goto/free discipline

Functions that allocate multiple resources follow this pattern:

1. `memset(out, 0, sizeof(*out))` — zero before any allocation so `free(NULL)`
   is safe on uninitialized pointers.
2. Sequential allocations, each guarded by
   `if (ptr == NULL) { returncode = BIOSIM_ERR_NOMEM; goto exit; }`.
3. Single `exit:` label: if `returncode != BIOSIM_OK`, call the free function
   (NULL-safe), then log the error.
4. Free functions tolerate NULL on every member and NULL on the struct itself.

## Error logging discipline

- Functions that exclusively return `BIOSIM_ERR_NOMEM` do **not** log.
- All other functions log once at `exit:` on failure:
  `BIOSIM_ERRORF("... (%s)", biosim_strerror(returncode))`.
- Callers do not re-log errors received from callees.

## No mutable global state

All state is passed by parameter. No file-scope mutable variables, no
singletons. `core` is called from both the single-threaded stepper and the
GPU host thread.

## Critical: host/device portability

Five headers are prepended to every OpenCL kernel program:
`grid_defs.h`, `rng.h`, `gene.h`, `io_defs.h`, `challenge_defs.h`.

These headers **must** compile as both C11 and OpenCL C:

- **No** `<stdio.h>`, `<stdlib.h>`, `<string.h>`, or any other host-only
  standard header.
- **No** heap allocation, function pointers, or pointer-member structs.
- Use the `_defs.h` naming for device-portable headers (type/enum/macro only).
- Every `_defs.h` carries `/* HOST/DEVICE: ... */` prologue — respect it.
- Host-only headers carry `/* HOST-ONLY: ... */`.

Violating these rules breaks kernel compilation at runtime, not at
`cmake --build` time.

## Testing

Each `src/foo.c` gets a mirror `tests/test_foo.c`. Register it in
`tests/CMakeLists.txt` and link against `unity` and the package under test.
Shared test helpers: `tests/sim_test_utils.{c,h}`.

## Further reading

- `docs/conventions-c.md` — full C/OpenCL conventions and portability pitfalls
- `docs/build-native.md` — native build setup
- `docs/build-opencl.md` — OpenCL runtime (needed to run sim-gpu tests)
- `docs/architecture.md` — key types and functions
