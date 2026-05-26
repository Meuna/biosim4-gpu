# sim-wasm

Emscripten WASM bindings. A single source file (`src/bindings.c`) compiles to
`biosim.mjs` + `biosim.wasm` — an ES6 WebAssembly module consumed by `webapp`
via a Web Worker. Emscripten flags: `-sMODULARIZE=1 -sEXPORT_ES6=1
-sENVIRONMENT=worker`. Depends on `core`.

See `packages/sim-wasm/README.md` for the full exported API.

**Build branch: Webapp.** Despite being C code, all quality-check steps use the
webapp preset — there is no separate WASM CMake preset.

## Quality check sequence

```sh
cmake --build --preset webapp
bun run --cwd packages/webapp test
cmake --build --preset webapp --target lint
cmake --build --preset webapp --target format
cmake --build --preset webapp
bun run --cwd packages/webapp test
```

A task is **NOT** complete while lint reports any error or warning.

## Naming

- **Files**: `snake_case.c`, `snake_case.h`
- **Public API**: `biosim_` prefix; **types**: `biosim_*_t`

## Error handling

Return `biosim_status_t` from functions that can fail; use
`biosim_strerror(code)` in error messages.

## Exported functions

All functions callable from JavaScript must be marked `BIOSIM_KEEPALIVE`, which
expands to `EMSCRIPTEN_KEEPALIVE` under Emscripten and is empty otherwise.

Return types must be simple C types (int, float, void). Structs are passed by
pointer to the Emscripten heap and accessed via `HEAP32`, `HEAPU8`, etc. on the
JavaScript side.

## Portability

`bindings.c` includes `core` headers; follow the host/device portability rules
from `docs/conventions-c.md`. Avoid POSIX file I/O and any host-only includes
that conflict with the Emscripten WASM environment.

## Further reading

- `docs/conventions-c.md` — full C/OpenCL conventions and portability pitfalls
- `docs/build-wasm.md` — Emscripten SDK setup and WASM build commands
- `docs/architecture.md` — sim-wasm architecture
