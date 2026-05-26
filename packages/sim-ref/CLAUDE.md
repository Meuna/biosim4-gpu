# sim-ref

Executable. Deterministic CPU reference simulator. A thin orchestration shell —
all simulation logic lives in `core`; `sim-ref` adds only the parameter table
and the two-level generation/step loop.

Agents are processed in increasing-index order. Conflicts are resolved
first-come-first-served. This is the reference baseline for verifying GPU
results.

**Build branch: Native.** Depends on `core` and `cfgparse`.

## Quality check sequence

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
- **Public API**: `biosim_` prefix; **types**: `biosim_*_t`
- **Section separators**: 79-char banner with U+2500 in multi-section `.c` files

## Error handling and logging

- Return `biosim_status_t`; use `biosim_strerror(code)` in messages.
- Functions log once at `exit:` on failure; callers do not re-log.

## Testing

Any helper functions extracted from `main.c` get a mirror test in
`packages/sim-ref/tests/test_foo.c` with a new `tests/CMakeLists.txt`.

## Further reading

- `docs/conventions-c.md` — full C/OpenCL conventions
- `docs/build-native.md` — native build setup
- `docs/usage.md` — CLI reference
- `docs/architecture.md` — sim-ref architecture and main-loop pattern
