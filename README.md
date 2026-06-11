<div align="center">

<img src="packages/webapp/public/favicon.svg" width="96" alt="biosim4-gpu logo">

# biosim4-gpu

**Evolve neural-net agents on a 2D grid — on your GPU, in your browser.**

A GPU-accelerated port of [biosim4](https://github.com/davidrmiller/biosim4)
(David Miller's evolutionary simulator of neural-net-driven agents) to
C11/OpenCL, with a CPU reference simulator and a Svelte web front-end.

[**▶ Try it live**](#) &nbsp;·&nbsp; placeholder — a hosted instance is coming soon

</div>

---

<div align="center">

<!-- TODO(#109 follow-up): replace this note with a real screenshot/animation:
     <img src="docs/assets/webapp.png" width="820" alt="biosim4-gpu webapp"> -->

📸 _Webapp screenshot/animation coming soon — build and run the webapp (see
[Getting started](#getting-started)) to see agents evolve live._

</div>

## What it is

biosim4 evolves small populations of agents whose behaviour is driven by an
evolved neural network. Each generation, agents that satisfy a *challenge*
(reach a zone, avoid a barrier, cluster together…) survive and reproduce, and
the population's behaviour emerges over hundreds of generations.

This port pushes the heavy per-step simulation onto the GPU and wraps the whole
thing in a browser UI, so you can **design a run, watch it evolve, and inspect
an individual agent's brain** — then hand the same configuration to the native
CLI to power through generations at full speed.

## Features

- **GPU-accelerated evolution** — the per-step pipeline runs as OpenCL kernels;
  a single-threaded CPU reference simulator (`sim-ref`) mirrors it for
  correctness.
- **Browser front-end** — configure population, grid, genome, challenges and
  barriers; watch generations render live on a canvas.
- **Agent inspection** — pick any agent and explore its evolved neural network
  as an interactive force-directed graph (the *brain explorer*).
- **Round-trip with the CLI** — export a `.toml` config and a `.snap` snapshot
  from the webapp and feed them straight to `biosim-gpu`; load a snapshot back
  into the webapp to replay the survivors.

## Getting started

### Webapp

**Prerequisites:** [Emscripten](https://emscripten.org) (`EMSDK` set), Bun, and
the build basics below.

```sh
# Build the WASM module + start the Vite dev server
cmake --preset webapp
cmake --build --preset webapp --target dev
# open http://localhost:5173
```

Design a run in the UI, hit play, and watch it evolve. Use the export buttons to
download a `.toml` config and a `.snap` snapshot for the native CLI.

### Native CLI

**Prerequisites:** C compiler, CMake 3.28+, Ninja, pkg-config, and vcpkg. An
OpenCL runtime is required to run `biosim-gpu` (see
[`docs/build-opencl.md`](docs/build-opencl.md)).

```sh
git clone https://github.com/Meuna/biosim4-gpu.git
cd biosim4-gpu

# Set up vcpkg (once)
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# Build
cmake --preset debug
cmake --build --preset debug

# CPU reference simulator (defaults: 3000 agents, 128×128 grid, 1000 generations)
./build/debug/packages/sim-ref/biosim-ref --config my_run.toml

# GPU simulator — feed a webapp-exported config and snapshot
./build/debug/packages/sim-gpu/biosim-gpu \
    --config my_run.toml --snapshot-in survivors.snap --snapshot-out out.snap
```

See [`docs/usage.md`](docs/usage.md) for the full CLI reference (parameters,
challenges, barriers, OpenCL device selection).

## Motivation

The port is 100% AI-assisted, which is the main educational objective: for
better or worse, I think AI is here to stay, and I want a hands-on (or
hands-off) experience building complex software with it. The approach is
documented in the [AI-development notes](docs/ai-development.md).

The secondary motivations are also educational: GPU acceleration, and
structuring a complex C program. Hopefully the project also accelerates the
biosim4 simulator and unlocks playing with more advanced behaviours.

## Repository map

| Path | Contents |
|------|----------|
| `packages/core/` | Simulation logic (genome, nnet, agents, grid, challenges, snapshot) |
| `packages/cfgparse/` | CLI/TOML/parameter management |
| `packages/sim-ref/` | Single-threaded CPU reference simulator |
| `packages/sim-gpu/` | OpenCL GPU simulator |
| `packages/sim-wasm/` | `core` compiled to a WebAssembly module |
| `packages/webapp/` | Svelte web front-end |
| `docs/architecture.md` | Package structure, key types and functions |
| `docs/build.md` | Prerequisites, build steps, lint/format targets |
| `docs/usage.md` | CLI reference, TOML format, challenges, barriers |
| `docs/gpu-design.md` | GPU kernel pipeline design |
| `docs/formats.md` | Snapshot binary format, TOML format overview |
| `STATUS.md` | Implementation status, missing parts, open design questions |

## License

MIT.
