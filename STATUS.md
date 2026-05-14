# Status

## Implementation

| Package | Status | Notes |
|---------|--------|-------|
| `core` | Complete | Simulation logic, genome, nnet, agents, grid, challenges, snapshot |
| `params` | Complete | CLI/TOML/parameter management |
| `sim-stepper` | Complete | Single-threaded CPU reference simulator |
| `sim-gpu` | In progress | K1–K5 complete: feedforward (sensors, nnet, actions), kill-marked grid cleanup, movement resolution, signal fade, challenge eval. Generation loop pending. |
| `viz` | Not started | Depends on stepper trace format (not yet defined) |

## Missing or incomplete

- **GPU pipeline**: K1–K5 complete — feedforward (sensors, nnet, actions, movement finalization), kill-marked grid cleanup, movement resolution, signal fade, and per-step challenge evaluation. Five-kernel per-step pipeline documented in [`docs/gpu-design.md`](docs/gpu-design.md). Generation loop (survivor selection, reproduction, respawn) pending.
- **Visualization**: `viz` package not started; trace format not yet defined.
- **CI/CD**: No `.github/` workflows.
- **Altruism challenge**: Placeholder — evaluator always returns `{false, 0.0f}`. Requires genome similarity computation not yet designed for GPU.
- **Benchmark harness**: No `benchmarks/` directory.
- **Developer tools**: No `tools/` directory (`snapshot-inspect`, `genome-dump`).

## Open GPU design questions

These were deferred from the initial GPU data-model design. Each requires a decision before the remaining `sim-gpu` kernel phases are implemented.

1. **Genome-length divergence**: sort-then-iterate vs. uniform-loop-with-predicate — when does each win?

2. **`GENETIC_SIM_FWD` fingerprint choice**: SimHash, MinHash-to-64, random-projection LSH, or handcrafted Hamming-friendly hash — evaluate correlation with true genome similarity under typical mutation patterns.

3. **Movement `atomic_cmpxchg` correctness**: verify the two-step claim-new / clear-old pattern under all concurrent scenarios; evaluate alternatives (sort-based, two-pass conflict detection).

4. **Signal emit — local-memory staging**: quantify reduction in global atomic contention from per-work-group tiling; choose work-group size and tile geometry.

5. **Grid and signal `image2d_t` binding**: confirm driver support, evaluate coalescing/cache benefit on realistic neighborhood access patterns.

6. **RNG choice and seeding**: xorshift64 vs. PCG vs. Philox; per-agent state management across generations; reproducibility-for-testing trade-off.

7. **Kernel fusion**: whether K1+K2 or K2+K3 should be merged given launch overhead on target hardware.

8. **Sort strategies**: genome-length sort, spatial sort, hybrid — frequency, algorithm (radix, bitonic), index-remapping bookkeeping.

9. **Challenge migration**: which challenges move cleanly to GPU vs. require host-side handling.

10. **Host-device data transfer at generation boundary**: pinned buffers, async transfers overlapping with reproduction compute.

11. **Per-sim.step host flow**: command queue construction, profiling events, debugging without per-step readbacks.
