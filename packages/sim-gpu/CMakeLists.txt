find_package(OpenCL REQUIRED)

# ── sim-gpu-lib — registry and runner (testable static library) ────────────

add_library(sim-gpu-lib STATIC
    src/registry.c
    src/runner.c
    src/pipeline.c
    src/benchmark.c
)
target_compile_features(sim-gpu-lib PUBLIC c_std_11)
target_include_directories(sim-gpu-lib PUBLIC include)
target_link_libraries(sim-gpu-lib PUBLIC core OpenCL::OpenCL)
# Silence the "CL_TARGET_OPENCL_VERSION not defined" pragma message and
# opt out of deprecated pre-2.0 API symbols.
target_compile_definitions(sim-gpu-lib PUBLIC CL_TARGET_OPENCL_VERSION=300)

# Embed portable headers as preamble strings (always loaded before any kernel).
# Generated files define: const char <var>[] = "...";
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_SOURCE_DIR}/packages/core/include/biosim/core/grid_defs.h"
    biosim_gpu_preamble_grid_defs_h
)
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_SOURCE_DIR}/packages/core/include/biosim/core/rng.h"
    biosim_gpu_preamble_rng_h
)
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_SOURCE_DIR}/packages/core/include/biosim/core/gene.h"
    biosim_gpu_preamble_gene_h
)
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_SOURCE_DIR}/packages/core/include/biosim/core/io_defs.h"
    biosim_gpu_preamble_io_defs_h
)
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_SOURCE_DIR}/packages/core/include/biosim/core/challenge_defs.h"
    biosim_gpu_preamble_challenge_defs_h
)

# Embed the K1 kernel source as a fallback string.
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels/k1_feedforward.cl"
    biosim_gpu_k1_feedforward
)

# Embed the K2 kernel source as a fallback string.
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels/k2_kill_marked.cl"
    biosim_gpu_k2_kill_marked
)

# Embed the K3 kernel source as a fallback string.
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels/k3_movement_resolution.cl"
    biosim_gpu_k3_movement_resolution
)

# Embed the K4 kernel source as a fallback string.
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels/k4_signal_fade.cl"
    biosim_gpu_k4_signal_fade
)

# Embed the K5 kernel source as a fallback string.
biosim_embed_opencl_source(sim-gpu-lib
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels/k5_challenge_step_eval.cl"
    biosim_gpu_k5_challenge_step_eval
)

# ── biosim-gpu — the OpenCL simulator executable ───────────────────────────

add_executable(biosim-gpu src/main.c)
target_compile_features(biosim-gpu PRIVATE c_std_11)
target_link_libraries(biosim-gpu PRIVATE sim-gpu-lib cfgparse core OpenCL::OpenCL
    $<$<PLATFORM_ID:Linux>:m>)
target_compile_definitions(biosim-gpu PRIVATE BIOSIM_PROGNAME="biosim-gpu")
biosim_target_git_version(biosim-gpu)
install(TARGETS biosim-gpu RUNTIME)

# ── biosim-gpu-bench — developer profiling benchmark (not installed) ────────

add_executable(biosim-gpu-bench src/bench_main.c)
target_compile_features(biosim-gpu-bench PRIVATE c_std_11)
target_link_libraries(biosim-gpu-bench PRIVATE sim-gpu-lib cfgparse core OpenCL::OpenCL
    $<$<PLATFORM_ID:Linux>:m>)
target_compile_definitions(biosim-gpu-bench PRIVATE BIOSIM_PROGNAME="biosim-gpu-bench")
biosim_target_git_version(biosim-gpu-bench)

if(BIOSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
