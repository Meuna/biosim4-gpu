# webapp

Svelte 5 single-page application that loads the biosim4-gpu WebAssembly module
(`sim-wasm`) inside a Web Worker. The WASM artifacts (`biosim.mjs` + `biosim.wasm`)
must be built first with `cmake --build --preset webapp` before starting the dev server.

## Prerequisites

- [Bun](https://bun.sh) — install with `curl -fsSL https://bun.sh/install | bash`
- [EMSDK](https://emscripten.org/docs/getting_started/downloads.html) — for building the WASM module

## Dev workflow

```sh
# 1. Build the WASM module and the webapp (from the repo root)
cmake --preset webapp
cmake --build --preset webapp

# 2. Start the Vite dev server (serves on http://localhost:5173)
cmake --build --preset webapp --target dev
```

Open the URL and check the browser JavaScript console for the log output from
the WASM module.
