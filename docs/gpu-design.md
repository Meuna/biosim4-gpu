# GPU Data Model Design

**Status:** K1 (`k_feedforward`) implemented. K2/K3 designed, not yet
implemented. See [`STATUS.md`](../STATUS.md).

This document describes the planned GPU/OpenCL architecture for `sim-gpu`. It
is the reference for implementing that package. The single-threaded reference
implementation in `sim-stepper` follows the same data model principles where
they apply to CPU code.

Companion document: [`docs/legacy-data-model.md`](legacy-data-model.md) —
the original C++/AoS model this design replaces.  
GPU concepts glossary: [`docs/gpu-primer.md`](gpu-primer.md).

## Design principles

1. **Everything per-agent is SoA.** One flat `__global` buffer per field,
   indexed by agent ID. No nested containers, no per-agent heap blocks.

2. **Variable-length per-agent structures are padded to fixed capacity.** Genomes
   and neural networks each get a fixed slot width equal to the worst case.
   Unused slots are marked inactive.

3. **Per-agent variable-length buffers are transposed.** Instead of
   `genome[agent_i][gene_j]`, storage is `genome[gene_j][agent_i]`. When all
   work-items advance to gene `j` together, they read contiguous memory —
   coalesced.

4. **The grid is a flat 2D buffer**, optionally exposed as `image2d_t` for
   read-only sensor access.

5. **Signals are `uint32_t` per cell.** Eliminates 8-bit atomic portability
   issues at the cost of 4× signal memory (negligible in absolute terms).

6. **`GENETIC_SIM_FWD` replaced by a 64-bit genome fingerprint.** Precomputed
   once per generation. Replaces variable-length cross-agent genome comparison
   with a single 8-byte load and a popcount.

7. **Move and death queues disappear.** Conflict resolution uses atomic CAS on
   grid cells and idempotent writes to alive flags. Reproducibility versus the
   CPU version is not preserved.

8. **Each simulation step is a small pipeline of kernels** separated by the
   kernel boundary (global barrier). One kernel per logically coherent phase.

9. **RNG state is per-agent, stored in SoA.** Each work-item draws from its
   own xorshift64 stream.

10. **The host owns the generation boundary.** Survivor selection, pairing,
    mutation, and respawn run on the host. This is the only cross-generation
    sequential phase explicitly accepted.

## Per-agent SoA buffers

Let `N = population` (dead agents keep their slot; slots reuse at the next generation).

| Buffer | Type | Notes |
|--------|------|-------|
| `alive[N]` | `uint8_t` | 0 = dead |
| `loc_x[N]`, `loc_y[N]` | `int32_t` | Split for independent coalesced access |
| `birth_x[N]`, `birth_y[N]` | `int32_t` | Used by challenge evaluation |
| `osc_period[N]` | `uint16_t` | OSC1 sensor |
| `responsiveness[N]` | `float` | Action gating |
| `long_probe_dist[N]` | `uint8_t` | Longprobe sensor range |
| `last_move_dir[N]` | `uint8_t` | Direction sensors |
| `kill_marker[N]` | `uint8_t` | Transient: K1 KILL_FORWARD sets; K2 clears grid cell |
| `challenge_bits[N]` | `uint32_t` | Per-challenge accumulator |
| `rng_state[N]` | `uint64_t` | Per-agent xorshift64 state |
| `genome_fingerprint[N]` | `uint64_t` | Precomputed; replaces `GENETIC_SIM_FWD` |
| `desired_x[N]`, `desired_y[N]` | `int32_t` | Transient: feedforward → movement kernel |

Total fixed per-agent footprint: ~40 bytes/agent. At `N = 4096`: ~160 KiB before genome and nnet.

## Genome storage

```c
// Two parallel buffers, gene-slot-major (transposed):
uint16_t genome_conn[GENOME_MAX_LEN * N];  // [gene_slot * N + agent]
int16_t  genome_wgt [GENOME_MAX_LEN * N];

uint8_t  genome_length[N];  // actual active gene count per agent
```

Transposed layout guarantees that all work-items reading gene `j` access
contiguous memory (coalesced). Padding waste: at `GENOME_MAX_LEN = 256`,
`N = 4096`, 4 bytes/gene → 4 MiB total; even 50% padding waste is negligible.

Agents sorted by descending `genome_length` after each generation boundary
reduces warp divergence in the genome-iteration loop.

## Neural network storage

Compiled connections stored in the same transposed SoA layout as the genome:

```c
uint16_t conn_packed[MAX_CONN * N];   // src/sink packed bits
int16_t  conn_weight[MAX_CONN * N];
uint16_t conn_length[N];

float    neuron_output[MAX_NEURONS * N];  // transposed: [neuron_k * N + agent]
uint8_t  neuron_driven[MAX_NEURONS * N];  // 1 if driven by input
uint8_t  neuron_count[N];
```

Connections whose sink is a neuron are placed before connections whose sink is
an action — the same invariant as the host implementation. This enables a
single-pass feedforward where accumulators stay in private (register) memory.

## Grid representation

```c
// GPU:
__global uint grid[SIZE_X * SIZE_Y];   // row-major: grid[y * SIZE_X + x]

// Host:
typedef struct { uint32_t *cells; int32_t size_x; int32_t size_y; } biosim_grid_t;
```

Cell encoding: `0` = empty, `0xFFFFFFFF` = barrier, otherwise agent index + 1 (`[1, 0xFFFFFFFE]`).
The host and GPU types match — no conversion is needed at host/device boundaries.

During the parallel phase the grid is read-only; binding it as `image2d_t`
enables the texture cache for neighborhood scans. A separate write pass
(movement resolution) uses the plain `__global` buffer.

## Signal representation

```c
__global uint32_t signal[SIZE_X * SIZE_Y];   // row-major
```

`uint32_t` per cell allows portable `atomic_add`. Value semantics stay 0–255;
the upper 24 bits are unused. Local-memory staging can reduce global atomic
contention for dense signal emission (deferred — see open questions in
[`STATUS.md`](../STATUS.md)).

## Kernel pipeline (per sim.step)

Five kernels per step, each separated by the kernel boundary (implicit global
barrier):

```
K1: feedforward_and_actions
    Reads:  alive, loc_*, osc_period, last_move_dir, responsiveness,
            long_probe_dist, genome_fingerprint, conn_packed, conn_weight,
            conn_length, neuron_output, neuron_driven, neuron_count,
            grid (read-only), signal (image2d_t), rng_state
    Writes: neuron_output (new), responsiveness, osc_period, long_probe_dist,
            desired_x, desired_y, alive (KILL_FORWARD targets — idempotent),
            kill_marker (KILL_FORWARD targets), signal (atomic_add), rng_state
    Size:   N work-items

K2: kill_marked
    Reads:  kill_marker, loc_*
    Writes: grid (atomic_cmpxchg — clears grid cells for kill-marked agents)
    Size:   N work-items

K3: movement_resolution
    Reads:  alive, loc_*, desired_x, desired_y
    Writes: loc_*, last_move_dir, grid (atomic_cmpxchg)
    Size:   N work-items

K4: signal_fade
    Reads:  signal
    Writes: signal (signal[c] = max(0, signal[c] - FADE_AMOUNT))
    Size:   SIZE_X * SIZE_Y work-items

K5: challenge_eval
    Reads:  alive, loc_*, birth_*, challenge_bits
    Writes: challenge_bits, alive (some challenges kill early)
    Size:   N work-items
```

Total: 5 kernel launches per step. At `steps_per_gen = 500`: 2,500 launches
per generation. Kernel fusion (K2+K3, K1+K2) is a later optimization.

## Queue-free conflict resolution

### Move conflicts

Each work-item atomically claims its target cell with `atomic_cmpxchg`. If the
cell is not empty, the move silently fails. No serial drain, no host-side
critical section. Reproducibility versus the CPU reference is not preserved —
which agent wins a contested cell depends on hardware scheduling.

### Death conflicts (`KILL_FORWARD`)

Writing `alive[target] = 0` and `kill_marker[target] = 1` is idempotent:
multiple killers of the same target produce the correct result without atomics.
K1 only stamps the marker; it does not touch the grid. K2 (`kill_marked`) then
sweeps `kill_marker` and clears each flagged agent's grid cell via
`atomic_cmpxchg`. This two-phase approach ensures the grid is a stable
read-only snapshot during K1, which is required by population-density sensors
that count occupied cells in the neighbourhood.

## Generation boundary (host-side)

At the end of each generation the host:

1. Reads back `alive[]` and survivor genome buffers.
2. Evaluates survivors (challenge-based fitness).
3. Pairs survivors and produces child genomes (crossover + mutation).
4. Compiles each child genome into a neural network.
5. Computes genome fingerprints.
6. Decides spawn locations.
7. Writes the full new population state to the device.

Keeping the boundary on the host means the GPU refactoring does not require
rewriting reproduction logic.

Data transfer at `N = 4096`, `GENOME_MAX_LEN = 256`: ~4 MiB per direction per
generation. Over PCIe Gen4 (~16 GB/s): ~0.5 ms — negligible versus a
generation's worth of sim.steps.

## Memory budget

For `N = 4096`, `SIZE_X = SIZE_Y = 128`, `GENOME_MAX_LEN = 256`, `MAX_NEURONS = 32`,
`MAX_CONN = 512`:

| Buffer | Size |
|--------|------|
| Per-agent fixed fields | ~160 KiB |
| Genome | 4 MiB |
| Neural net connections | 8 MiB |
| Neuron outputs | 512 KiB |
| Neuron driven flags | 128 KiB |
| Grid | 64 KiB |
| Signal | 64 KiB |
| Transient desired_x / desired_y | 32 KiB |
| **Total** | **~13 MiB** |

Scaling `N` to 65,536 yields ~200 MiB — trivial on any modern 4 GiB+ GPU.
