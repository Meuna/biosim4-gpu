# OpenCL Runtime Setup

The native build links against the Khronos OpenCL ICD Loader supplied by
vcpkg — the build itself does not need a runtime. A runtime is required only
to *run* `biosim-gpu` or its tests.

Use `clinfo` to confirm at least one OpenCL platform is visible:

```sh
sudo apt install clinfo   # diagnostic tool
clinfo
```

## Linux — install a runtime

The GPU kernel driver and the userspace OpenCL ICD are separate packages on
Linux.

- **NVIDIA** — the ICD (`nvidia.icd`) ships with the `libnvidia-compute`
  package that comes with the driver. See the
  [CUDA installation guide](https://developer.nvidia.com/cuda-downloads).
- **AMD** — install the [ROCm OpenCL runtime](https://rocm.docs.amd.com/) or,
  for integrated/older cards, Mesa's implementation:
  `sudo apt install mesa-opencl-icd`
- **Intel** — `sudo apt install intel-opencl-icd` (see the
  [Intel Compute Runtime](https://github.com/intel/compute-runtime)).

**CPU fallback (no GPU required):**

[POCL](http://portablecl.org/) provides an OpenCL 3.0 CPU driver and is
useful for development and CI without a physical GPU:

```sh
sudo apt install pocl-opencl-icd
```

## Windows

Install the official NVIDIA or AMD GPU driver. The OpenCL ICD is bundled with
the driver — no extra steps are needed.

## Building without GPU support

To build without the GPU simulator entirely:

```sh
cmake --preset debug -DBIOSIM_BUILD_GPU=OFF
```
