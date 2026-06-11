# Status

## Implementation

| Package | Status | Notes |
|---------|--------|-------|
| `core` | Complete | Simulation logic, genome, nnet, agents, grid, challenges, snapshot |
| `cfgparse` | Complete | CLI/TOML/parameter management |
| `sim-ref` | Complete | Single-threaded CPU reference simulator |
| `sim-gpu` | Functional | Runs full generations end-to-end. The per-step pipeline (K1–K5: feedforward, kill-marked grid cleanup, movement resolution, signal fade, challenge eval) runs on the GPU; survivor selection and reproduction run host-side at the generation boundary. Snapshot import/export wired into the main loop. |
| `sim-wasm` | Feature-complete | Lifecycle/stepping, by-name parameter setters, challenge-spec and barrier-list setters, snapshot import/export, and `nnet`/inspection buffer getters — the full surface the webapp drives. |
| `webapp` | Functional | Config panel for all scalar params, challenge spec, and barriers; live canvas rendering with agent inspection and a brain explorer wired to live `nnet` data; the simulation lifecycle is encapsulated in the `SimMachine` state machine (composite phase × dirty); TOML config and snapshot import/export round-trip with the native CLI. |

## Missing or incomplete

- **GPU-side reproduction**: the per-step pipeline (K1–K5) runs on the GPU, but survivor selection and reproduction run on the host at each generation boundary. Moving them onto the GPU is open — see the design questions below. The five-kernel per-step pipeline is documented in [`docs/gpu-design.md`](docs/gpu-design.md).
- **CI/CD**: native, webapp, and Windows quality-check workflows run on GitHub Actions; there is no deployment or publish step, so the webapp is not yet hosted.
- **Altruism challenge**: placeholder — the evaluator always returns `{false, 0.0f}`, and the `GENETIC_SIM_FWD` genome-similarity sensor stays commented out, pending a GPU-friendly similarity design.
- **Benchmark harness**: no `benchmarks/` directory.
- **Developer tools**: no `tools/` directory (`snapshot-inspect`, `genome-dump`).

## Open GPU design questions

These optimization decisions were deferred from the initial GPU data-model design. The per-step pipeline ships with a working baseline; each question concerns moving more work onto the GPU (reproduction, sorting, genome similarity) or hardening the existing kernels.

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
