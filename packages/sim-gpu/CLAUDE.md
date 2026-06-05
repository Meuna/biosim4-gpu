# sim-gpu

Static library (`sim-gpu-lib`) and executable (`biosim-gpu`). OpenCL GPU
simulator. The kernel registry provides two-level lookup (filesystem override →
embedded fallback via `EmbedKernels.cmake`). The runner manages the
platform/device/context/queue lifecycle. `biosim_gpu_pipeline_t` owns programs,
kernels, and buffers; it drives K1→K5 per simulation step.

Non-deterministic: movement conflict resolution uses `atomic_cmpxchg`; the
winner depends on hardware scheduling.

**Build branch: Native.** Depends on `core`, `cfgparse`, and OpenCL ICD (vcpkg).

## Quality check sequence

Running tests requires an OpenCL runtime — see `docs/build-opencl.md`.
Tests gracefully `IGNORE` (not fail) when no OpenCL platform is found; `ctest`
passes even without a GPU.

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

- **Files**: `snake_case.c`, `snake_case.h`; kernel files: `k_name.cl`
- **Public API**: `biosim_` prefix; **types**: `biosim_*_t`
- **Kernel entry points**: `k_` prefix only — e.g. `k_feedforward`, `k_movement`. No other scope prefix (`s_`, `g_`, etc.) on any symbol.
- **Section separators**: 79-char banner with U+2500 in multi-section `.c` files

## Error handling

Return `biosim_status_t`. Use `biosim_strerror(code)` in messages.

## Alloc/goto/free discipline

1. `memset(out, 0, sizeof(*out))` before any allocation.
2. Sequential allocations guarded by `goto exit` on failure.
3. Single `exit:` label: free on error, log the error.
4. Free functions tolerate NULL on every member and the struct itself.

## Error logging discipline

- Functions returning only `BIOSIM_ERR_NOMEM` do **not** log.
- All others log once at `exit:`: `BIOSIM_ERRORF("... (%s)", biosim_strerror(returncode))`.
- Callers do not re-log errors from `biosim_*` callees.

## OpenCL error handling

Always include `cl_macros.h` instead of OpenCL headers directly — it handles
the `#ifdef __APPLE__` include guard automatically.

Use the macros to reduce boilerplate and ensure consistent error reporting:

```c
CL_GOTO_EXIT_ON_ERROR(clEnqueueNDRangeKernel(...));
CL_ASSIGN_OR_GOTO_EXIT(program, clCreateProgramWithSource(..., &err));
```

Both macros log the failure with `BIOSIM_ERRORF(...)`, set
`returncode = BIOSIM_ERR_OPENCL`, and `goto exit`.

## Kernel authoring rules

- Kernel source files live in `packages/sim-gpu/kernels/`.
- The five portable headers (`grid_defs.h`, `rng.h`, `gene.h`, `io_defs.h`,
  `challenge_defs.h`) are prepended automatically by the registry — **do not**
  `#include` them in `.cl` files.
- No host-only types, no `<stdio.h>` / `<stdlib.h>` / `<string.h>` in kernels.
- Filesystem override: place a `.cl` file alongside the binary to hot-swap a
  kernel without rebuilding (useful during iteration).

## Testing

`src/foo.c` → `tests/test_foo.c`. Register in `tests/CMakeLists.txt`, link
against `unity` and `sim-gpu-lib`. Shared GPU test helpers:
`tests/gpu_test_utils.{c,h}`.

## Further reading

- `docs/conventions-c.md` — full C/OpenCL conventions and portability pitfalls
- `docs/build-native.md` — native build setup
- `docs/build-opencl.md` — OpenCL runtime installation
- `docs/gpu-design.md` — kernel pipeline design
- `docs/architecture.md` — sim-gpu architecture
