# cfgparse

Static library. CLI argument parsing (argtable3) and TOML config loading
(tomlc17). The parameter data model lives in `core/params.h`; `cfgparse` only
populates it. Public API entry point: `biosim_params_parse`. Depends on `core`.

Parameters are added to the exhaustive entry table defined in each simulator's
`main.c`, not inside `cfgparse` itself. `cfgparse` provides no shared defaults
and no extension mechanism.

**Build branch: Native.**

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
- **Public API**: `biosim_` prefix — e.g. `biosim_params_parse`
- **Types**: `biosim_*_t`
- **Section separators**: 79-char banner with U+2500 in multi-section `.c` files

## Error handling

Return `biosim_status_t`. Use `biosim_strerror(code)` in error messages.

## Alloc/goto/free discipline

1. `memset(out, 0, sizeof(*out))` before any allocation.
2. Sequential allocations guarded by `goto exit` on failure.
3. Single `exit:` label: free on error, log the error.
4. Free functions tolerate NULL on every member and the struct itself.

## Error logging discipline

- Functions returning only `BIOSIM_ERR_NOMEM` do **not** log.
- All others log once at `exit:`: `BIOSIM_ERRORF("... (%s)", biosim_strerror(returncode))`.
- Callers do not re-log errors from callees.

## Testing

`src/foo.c` → `tests/test_foo.c`. Register in `tests/CMakeLists.txt`, link
against `unity` and `cfgparse`.

## Further reading

- `docs/conventions-c.md` — full C/OpenCL conventions
- `docs/build-native.md` — native build setup
- `docs/formats.md` — TOML parameter format
- `docs/usage.md` — CLI parameter reference
