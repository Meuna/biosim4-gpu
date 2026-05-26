# WebAssembly Build Guide

Covers the `sim-wasm` package (Emscripten ES6 module).

## Prerequisites

Both Emscripten and Bun must be installed and on `PATH` before running the
webapp preset.

### Emscripten SDK

**Linux:**

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest
~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh      # sets EMSDK; add to ~/.bashrc
```

**Windows:**

Follow the [Emscripten SDK guide](https://emscripten.org/docs/getting_started/downloads.html).
Use the same `emsdk install` / `emsdk activate` steps; run `emsdk_env.bat`
instead of the `source` command.

### Bun

**Linux:**

```sh
curl -fsSL https://bun.sh/install | bash
# adds ~/.bun/bin to PATH; re-open your shell or source ~/.bashrc
```

**Windows:**

```sh
powershell -c "irm bun.sh/install.ps1 | iex"
```

## Configure and build

```sh
cmake --preset webapp
cmake --build --preset webapp
```

Output artifacts:

- `build/webapp/packages/sim-wasm/biosim.mjs` — Emscripten ES6 loader
- `build/webapp/packages/sim-wasm/biosim.wasm` — WebAssembly binary
- `packages/webapp/dist/` — bundled Svelte SPA (static files)

> The `webapp` preset builds both `sim-wasm` and the Svelte SPA. The WASM
> artifacts are copied into `packages/webapp/public/wasm/` before the Vite
> build runs. To work with only the WASM module, you can still use this preset
> — the Svelte build step is a no-op if the SPA has no changes.

## Next steps

For the webapp dev server, testing, and linting workflow, see
`docs/build-webapp.md`.
