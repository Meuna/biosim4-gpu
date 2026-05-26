# Webapp Build Guide

Covers the `webapp` package (Svelte 5 SPA).

## Prerequisites

Requires Emscripten and Bun installed and on `PATH`. See `docs/build-wasm.md`
for installation instructions. The WASM artifacts (`biosim.mjs`, `biosim.wasm`)
must be built before the dev server or production build.

## Configure and build

```sh
cmake --preset webapp
cmake --build --preset webapp
```

This builds both `sim-wasm` and the Svelte SPA. Output:

- `build/webapp/packages/sim-wasm/biosim.mjs` — Emscripten ES6 loader
- `build/webapp/packages/sim-wasm/biosim.wasm` — WebAssembly binary
- `packages/webapp/dist/` — bundled Svelte SPA (static files)

## Dev server

```sh
cmake --build --preset webapp --target dev
```

Opens a Vite dev server at `http://localhost:5173`.

## Test

```sh
bun run --cwd packages/webapp test          # non-interactive
bun run --cwd packages/webapp test:watch    # watch mode
```

Runs Vitest.

## Format and lint

```sh
cmake --build --preset webapp --target format  # Prettier
cmake --build --preset webapp --target lint    # ESLint + Prettier check
```

## Smoke test

```sh
python3 -m http.server 8080 --directory build/webapp/packages/webapp/dist
```

Open a browser at `http://localhost:8080`.
