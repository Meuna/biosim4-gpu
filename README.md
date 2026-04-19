# biosim4-gpu

A GPU-accelerated rewrite of [biosim4](https://github.com/davidrmiller/biosim4),
David Miller's evolutionary simulator of neural-net-driven agents on a 2D grid.
 
The project ports the simulation from a multi-threaded CPU implementation in
C++ (AoS layout, OpenMP) to a GPU/SoA architecture in C99/C11 with OpenCL
kernels. It also provides a second, single-threaded CPU simulator — the
*stepper* — that reproduces the same simulation step by step for visualization
and cross-validation.

## Motivation

The port is 100% AI-assisted which is the main educational objective: for better
or for worst, I think IA is here to stay and  I want a hands-on (or hands-off)
experience building a complex software using IA. The approach I used is documented
in the [design section](docs/design/README.md).

The next motivations for this project are also educational:

- GPU acceleration
- Structuring a complex C program

Hopefully, the project will also accelerate the biosim4 simulator and unlock
playing with advanced behaviors.

## Status
 
**Pre-implementation.** The repository currently contains only the design
documentation. No source code, build files, or tests exist yet. The next step
is to populate the structure described in
[`docs/design/01-repository-structure.md`](docs/design/01-repository-structure.md).

## Repository map
 
```
biosim4-gpu/
├── README.md                          ← this file
└── docs/
    └── design/
        ├── README.md                  ← describe the design approach
        ├── 01-repository-structure.md ← how this repo will be laid out
        ├── 02-implementation-conventions.md ← code layout invariants
        ├── 03-portable-build.md       ← how the build chain supports portability
        ├── 04-legacy-data-model       ← analysis of the legacy CPU/AoS model
        ├── 05-gpu-data-model.md       ← the proposed GPU/SoA model
        └── 06-external-icd.md         ← external interface formats (config, snapshot)
```

## Target platforms
 
- Linux x86-64 (primary development, w/o GPU)
- Windows x86-64 (first-class target, w/ GPU)
- Linux ARM64 (cloud testing with PoCL as the OpenCL CPU runtime)

## License
 
MIT.
