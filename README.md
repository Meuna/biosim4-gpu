# biosim4-gpu

A GPU-accelerated port of [biosim4](https://github.com/davidrmiller/biosim4)
(David Miller's evolutionary simulator of neural-net-driven agents on a 2D grid)
to C11/OpenCL, with a single-threaded CPU reference simulator.

The GPU simulator (`sim-gpu`) is designed but not yet implemented. The
single-threaded reference simulator (`sim-stepper`) is complete and produces
correct simulation results.

## Motivation

The port is 100% AI-assisted which is the main educational objective: for better
or for worse, I think IA is here to stay and  I want a hands-on (or hands-off)
experience building a complex software using IA. The approach I used is documented
in the [design section](docs/ai-development.md).

The next motivations for this project are also educational:

- GPU acceleration
- Structuring a complex C program

Hopefully, the project will also accelerate the biosim4 simulator and unlock
playing with advanced behaviors.

## Quick start

**Prerequisites:** C compiler, CMake 3.28+, Ninja, pkg-config. See [`docs/build.md`](docs/build.md).

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

# Run with defaults (3000 agents, 128×128 grid, 1000 generations)
./build/debug/packages/sim-stepper/biosim-stepper

# Run with a config file
./build/debug/packages/sim-stepper/biosim-stepper --config my_run.toml
```

## Repository map

| Path | Contents |
|------|----------|
| `packages/core/` | Simulation logic (genome, nnet, agents, grid, challenges, snapshot) |
| `packages/params/` | CLI/TOML/parameter management |
| `packages/sim-stepper/` | Single-threaded CPU simulator |
| `docs/architecture.md` | Package structure, key types and functions |
| `docs/build.md` | Prerequisites, build steps, lint/format targets |
| `docs/usage.md` | CLI reference, TOML format, challenges, barriers |
| `docs/conventions.md` | Naming, error handling, alloc/goto/free, portability |
| `docs/gpu-design.md` | Planned GPU architecture (not yet implemented) |
| `docs/formats.md` | Snapshot binary format, TOML format overview |
| `STATUS.md` | Implementation status, missing parts, open design questions |

## License

MIT.
