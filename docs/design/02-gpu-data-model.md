# BioSim4 — GPU/OpenCL Data Model Design (Refactoring Proposal)

**Status:** Step 1 of 2 — High-level architectural proposal.
**Companion document:** `biosim4-data-model-design.md` (current AoS model, used as the reference).
**Scope:** Memory layout, kernel decomposition, conflict resolution strategy, feature adjustments.
**Out of scope (deferred to Step 2):** Concrete OpenCL kernel code, host-device transfer patterns, specific atomic primitives, sorting implementation, fingerprint hash choice.

**Design assumptions (from the conversation):**
- Reproducibility may be sacrificed for performance.
- Dropping or replacing costly features is acceptable.
- Generation-boundary synchronization is accepted; within-step sequentiality should be minimized but not forcibly eliminated.

---

## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [GPU Terminology Primer](#2-gpu-terminology-primer)
3. [Design Principles](#3-design-principles)
4. [New Primitive and Spatial Types](#4-new-primitive-and-spatial-types)
5. [Per-Agent Buffers — Structure of Arrays](#5-per-agent-buffers--structure-of-arrays)
6. [Genome Storage](#6-genome-storage)
7. [Neural Network Storage](#7-neural-network-storage)
8. [Grid Representation](#8-grid-representation)
9. [Signal Representation](#9-signal-representation)
10. [Queue-Free Conflict Resolution](#10-queue-free-conflict-resolution)
11. [Sensor Catalogue — GPU-Adapted](#11-sensor-catalogue--gpu-adapted)
12. [Action Catalogue — GPU-Adapted](#12-action-catalogue--gpu-adapted)
13. [Simulation Loop — Kernel Breakdown](#13-simulation-loop--kernel-breakdown)
14. [Generation Boundary and Reproduction](#14-generation-boundary-and-reproduction)
15. [Feature Changes Summary](#15-feature-changes-summary)
16. [Memory Budget Estimate](#16-memory-budget-estimate)
17. [Deferred Decisions for Step 2](#17-deferred-decisions-for-step-2)

---

## 1. Goals and Non-Goals

### Goals

- Move the per-simStep workload (sensors → feedForward → actions) onto the GPU as one or a small number of OpenCL kernels.
- Replace the current pointer-heavy AoS layout with flat, coalesceable SoA buffers.
- Preserve the biological *behavior* of the simulation — the emergent dynamics — even where bit-for-bit reproducibility is lost.
- Keep the generation boundary (reproduction, mutation, respawn) on the host side for simplicity. Not all of it has to stay on the host, but none of it *has to* move to the GPU in the first iteration.

### Non-Goals

- Bit-for-bit reproducibility versus the current CPU implementation.
- Multi-GPU execution.
- Asynchronous streaming of generations (running generation N+1 while finalizing N).
- Support for GPUs below OpenCL 1.2 with no 32-bit atomic capability. The design assumes at minimum OpenCL 1.2 with `cl_khr_global_int32_base_atomics` and `cl_khr_local_int32_base_atomics`. OpenCL 2.0+ is preferred for generic atomics.

---

## 2. GPU Terminology Primer

Before presenting the design, this section builds up the vocabulary we will use to justify every choice. If a concept is already familiar, skip it.

### 2.1 Execution model: work-items, work-groups, wavefronts

OpenCL launches a kernel across an **NDRange**: a 1D, 2D, or 3D grid of **work-items**. A work-item is the GPU equivalent of a thread. Each work-item runs the same kernel code but typically with a different index (`get_global_id(0)`), and therefore typically touches a different slice of memory.

Work-items are clustered into **work-groups**. All work-items in the same work-group run on the same compute unit (SM in NVIDIA terms, CU in AMD terms). Only within a work-group is cheap synchronization available: `barrier(CLK_LOCAL_MEM_FENCE)` waits for every work-item in the group to reach that line.

Inside a work-group, the hardware further clusters work-items into **wavefronts** (AMD term) or **warps** (NVIDIA term). Wavefronts are typically 32 (NVIDIA), 32 or 64 (AMD), 8/16/32 (Intel). A wavefront executes **in lockstep**: all 32 lanes execute the same instruction at the same clock cycle. This is called **SIMT** (Single Instruction, Multiple Threads).

**Why this matters:** The wavefront is the atomic unit of GPU execution. The entire wavefront pays the cost of its slowest lane. If 31 lanes have finished and 1 lane is still running a loop, the other 31 sit idle but still consume execution slots.

### 2.2 Warp (wavefront) divergence and coherence

**Warp divergence** occurs when work-items within the same wavefront take different code paths. Because all 32 lanes must execute the same instruction, divergence is handled by **predication**: the hardware masks off the lanes that don't want to execute the current branch, runs the `if` branch with only the "true" lanes active, then runs the `else` branch with only the "false" lanes active. Both branches run serially. The cost is roughly additive.

Concretely:
- `if (x > 0) { work_A() } else { work_B() }` — if half the lanes take each branch, both `work_A` and `work_B` execute on the full wavefront, but with half the lanes masked during each. Effective throughput is halved.
- `for (i = 0; i < agent_specific_length; ++i) { ... }` — the wavefront's loop runs for as many iterations as the *longest* lane needs. Short-lane lanes sit idle for the remaining iterations.

**Warp coherence** is the opposite: all lanes in the wavefront take the same path and execute the same number of iterations. The full throughput of the wavefront is realized.

**Design lever:** Any time variable-length iteration appears per-agent, we can buy back coherence by *sorting* agents so that similar-length agents end up in the same wavefront. Adjacent thread indices land in the same wavefront, so if agents with similar genome length have adjacent indices, they diverge less.

### 2.3 Memory coalescing

When a wavefront issues a memory load, the hardware does not issue 32 separate transactions. It looks at the 32 addresses and groups them into **memory transactions** of 32, 64, or 128 bytes aligned on the natural boundary. If all 32 addresses fall into a single 128-byte aligned segment, it is **one** transaction — this is **coalesced access**.

- Lane 0 reads `buf[0]`, lane 1 reads `buf[1]`, …, lane 31 reads `buf[31]` (with `buf` being 4-byte floats) → 128 bytes contiguous → 1 transaction. Perfectly coalesced.
- Lane 0 reads `buf[0]`, lane 1 reads `buf[1024]`, lane 2 reads `buf[2048]`, … → 32 scattered transactions. Effective bandwidth divided by 32.

**Design lever:** Lay out data so that *adjacent work-items read adjacent addresses* when they read "the same logical field". In our case, if work-item `i` processes agent `i`, then `agent[i].age` must live at address `base_age + i * sizeof(uint32_t)` (SoA), **not** at `base + i * sizeof(Indiv) + offset_of_age` (AoS with wide stride).

### 2.4 Pointer chasing

A **pointer-chased** access pattern means following a pointer to reach data. Example from the current CPU model: to read an agent's first gene, we have `Indiv → genome.data() pointer → heap block → gene[0]`. The CPU can often hide this behind prefetchers and deep caches. The GPU cannot: each pointer dereference is a full memory round-trip (~400–800 cycles on GDDR6, much worse than arithmetic).

Moreover, pointer-chased accesses are **never coalesced**: adjacent work-items that dereference their own pointer end up reading 32 scattered heap locations, blowing the memory subsystem.

**Design lever:** Flatten everything. No nested `std::vector`, no per-agent heap blocks. One big contiguous buffer per logical field, indexed by agent ID.

### 2.5 Memory locality — spatial and temporal

- **Spatial locality:** After touching address `A`, a nearby address (`A + 1`, `A + 64`) is cheap because it likely came in the same cache line or memory transaction.
- **Temporal locality:** After touching address `A`, touching it again soon is cheap because it's still in cache.

GPU caches are small per compute unit (L1 of tens of KB, L2 of a few MB shared). Relying on temporal locality is weaker than on a CPU. Spatial locality, however, is *the* primary optimization target because it drives coalescing.

### 2.6 AoS, SoA, and AoSoA

- **AoS (Array of Structures):** `struct Indiv { uint32_t age; Coord loc; ... }` then `Indiv indivs[N]`. Reading `indivs[i].age` for all `i` is strided by `sizeof(Indiv)` — anti-coalesced. Cache-friendly on the CPU, toxic on the GPU.
- **SoA (Structure of Arrays):** `struct Population { uint32_t age[N]; int16_t loc_x[N]; int16_t loc_y[N]; ... }`. Reading `age[i]` for all `i` is contiguous — perfectly coalesced. This is the GPU standard.
- **AoSoA (hybrid):** Group fields into small structs but block the arrays by wavefront size. Rarely worth the complexity unless a kernel consistently co-reads several fields.

We use **pure SoA** for all per-agent state. AoSoA is mentioned where it might be a later optimization but is not chosen up front.

### 2.7 Atomics and contention

**Atomic operations** (atomic_add, atomic_cmpxchg, atomic_max, …) let multiple work-items modify a shared location with a consistent read-modify-write sequence. They are essential for cases like "many agents want to write to the same signal cell".

Costs:
- An uncontended atomic is roughly as expensive as a normal global memory write.
- A **contended** atomic (many work-items hitting the same address in the same cycle) serializes. Contention of 32 lanes on one address roughly costs 32× an uncontended write.
- Byte-wide (`uint8_t`) atomics are **not universally supported**. Portable code uses 32-bit atomics. If the value naturally fits in a byte, either promote it to 32 bits or do a packed CAS loop over a 4-byte word containing the byte. Promotion is simpler.

**Design lever:** Where we must write contended data (signals, shared counters), use 32-bit atomics. Where possible, reduce contention by aggregating writes in **local memory** (per work-group scratchpad) first, then flushing to global memory.

### 2.8 Local memory and barriers

**Local memory** (`__local` in OpenCL) is a small, fast, per-work-group scratchpad, typically 16–64 KB. Access latency is close to a register. All work-items in the same work-group share it. It is the ideal place to:
- Stage reductions before writing to global memory.
- Cache read-only data that every work-item in the group will read.
- Build per-group histograms, per-group atomic counters that are then reduced once to global memory.

`barrier(CLK_LOCAL_MEM_FENCE)` synchronizes all work-items in the work-group, ensuring local memory writes are visible. It is cheap. There is **no portable barrier across work-groups** inside a kernel launch — global synchronization requires ending the kernel and starting a new one.

### 2.9 Image objects (texture memory)

OpenCL `image2d_t` objects go through a dedicated **texture cache** with hardware-cached 2D spatial locality. Reading a grid cell and its 8 neighbors through an `image2d_t` is much friendlier than a naked `__global` buffer of identical content. For the grid and possibly the signal layer, image objects are a natural fit *for read-only access*.

Caveat: writing to an image during the same kernel launch that reads it is restricted. Read-only-during-parallel-phase is exactly our model, so this is fine.

### 2.10 Host-device transfer

Anything stored in host RAM must cross PCIe (or NVLink, or integrated memory) to reach the GPU. This is the highest-latency, lowest-bandwidth link in the system. **Every simStep that transfers data is a performance death sentence.** The refactoring must keep all per-simStep state resident on the GPU for the whole generation. Only at the generation boundary do we read back what the host needs (survivor flags, possibly genomes) and write back the new population.

---

## 3. Design Principles

Crystallized from the pitfalls identified in Section 14 of the source document and the GPU concepts in Section 2:

1. **Everything per-agent is SoA.** One flat `__global` buffer per field, indexed by agent ID. No nested containers, no per-agent heap blocks, no pointers to private data.

2. **Every per-agent variable-length structure is padded to a fixed capacity.** Genomes and neural networks each get a fixed slot width equal to the worst case. Unused slots are marked inactive. Memory waste is accepted in exchange for coalesced, pointer-free access.

3. **Per-agent variable-length buffers are transposed.** Instead of `genome[agent_i][gene_j]`, storage is `genome[gene_j][agent_i]`. When all work-items simultaneously read their gene `j`, they read contiguous memory — coalesced.

4. **The grid is a flat 2D array.** Optionally exposed as `image2d_t` for read-only sensor access during the parallel phase.

5. **Signals are promoted to `uint32_t` per cell.** This eliminates the byte-atomic portability issue at the cost of 4× signal memory (small in absolute terms).

6. **GENETIC_SIM_FWD is replaced by a pre-computed 64-bit genome fingerprint.** Variable-length cross-agent genome comparison is replaced by a single 8-byte load + fast bit similarity metric. This is a behavioral change, not equivalence.

7. **Move and death queues disappear.** Conflict resolution is folded into the parallel phase using atomic CAS on grid cells and atomic writes on alive flags. Reproducibility is sacrificed; determinism vs the CPU version is lost.

8. **Each simulation step is a small pipeline of kernels separated by a single global barrier** (the kernel boundary itself). One kernel per logically coherent phase.

9. **RNG state is per-agent, stored in SoA.** Each work-item draws from its own stream. A fast xorshift-family generator is sufficient.

10. **The host owns the generation boundary.** Survivor selection, pairing, mutation, and respawn run on the host at first. This is the only cross-generation sequential phase we explicitly accept.

---

## 4. New Primitive and Spatial Types

The existing types are fine for concept; they are re-expressed here as GPU-friendly POD types. All types are plain value types — no methods stored in memory, no virtual tables. Helper functions become free functions (or OpenCL `inline` functions).

### 4.1 `Coord` — 4 bytes (unchanged)

```cpp
struct Coord {
    int16_t x;
    int16_t y;
};
```

Stored as two separate SoA buffers when used per-agent: `int16_t loc_x[N]`, `int16_t loc_y[N]`. Packed back into a `Coord` inside the kernel when convenient.

**Rationale for splitting into two buffers:** Many sensors read only `x` or only `y` (the `LOC_X`, `LOC_Y`, `BOUNDARY_DIST_X` sensors). Splitting buys independent coalesced access. Sensors that read both pay at most one extra transaction.

### 4.2 `Dir` — stored as `uint8_t`

Same 9-value compass. Stored as a dedicated `uint8_t lastMoveDir[N]` SoA buffer. Single-byte loads are coalesced into 32-byte transactions on the GPU.

### 4.3 `Gene` — 6 bytes packed → promoted to 8 bytes

```cpp
struct Gene {
    uint16_t src_and_sink;  // packed: [1 src_type][7 src_num][1 sink_type][7 sink_num]
    int16_t  weight;
    uint32_t _pad;          // alignment to 8 bytes
};
```

**Why pad to 8 bytes?** Memory transactions are most efficient on power-of-two-aligned payloads. A 6-byte stride across an array causes some work-items to straddle cache-line boundaries. Padding to 8 bytes wastes 25 % but guarantees that `gene[j]` for 32 consecutive agents lands in a single 256-byte transaction.

Alternative: **split `Gene` itself into two SoA buffers** — one for the packed connectivity bits, one for the weight — so two 32-bit buffers, no padding waste, still perfectly coalesced. This is likely the better choice and is the default in this design.

```cpp
// Per-gene-slot, per-agent:
uint16_t genome_connectivity[GENOME_MAX_LENGTH][N];   // the src/sink bits
int16_t  genome_weight[GENOME_MAX_LENGTH][N];         // the raw weight
```

This is the core "transposed, padded SoA" layout — see Section 6.

### 4.4 `Neuron::output` — `float`, SoA

```cpp
float neuron_output[MAX_NEURONS][N];
```

Transposed just like the genome.

The `driven` flag is not stored per-step: it is a compile-time property of the neural network layout. We store it once per agent's neural net (see Section 7).

### 4.5 `Polar` — not stored

Transient, computed inline in kernel private memory, dropped at the end of the work-item. No buffer.

---

## 5. Per-Agent Buffers — Structure of Arrays

All per-agent fixed-size fields become their own flat `__global` buffer of length `POPULATION`. The grouping into an `Indiv` struct is abandoned.

### 5.1 Buffer table

Let `N = POPULATION` (maximum population; dead agents are kept in-place with the `alive` flag set to 0, the slot is reused only at the next generation).

| Buffer | Element type | Size (bytes) | Read phase | Written phase |
|---|---|---|---|---|
| `alive[N]` | `uint8_t` | N | Sensors, skip guard | Death resolution kernel |
| `loc_x[N]` | `int16_t` | 2N | Sensors, movement | Movement resolution kernel |
| `loc_y[N]` | `int16_t` | 2N | Sensors, movement | Movement resolution kernel |
| `birth_x[N]` | `int16_t` | 2N | Challenge eval | Generation spawn (host) |
| `birth_y[N]` | `int16_t` | 2N | Challenge eval | Generation spawn (host) |
| `age[N]` | `uint16_t` | 2N | AGE sensor | feedForward kernel (self) |
| `osc_period[N]` | `uint16_t` | 2N | OSC1 sensor | feedForward kernel (self) |
| `responsiveness[N]` | `float` | 4N | Action execution | feedForward kernel (self) |
| `long_probe_dist[N]` | `uint8_t` | N | Longprobe sensors | feedForward kernel (self) |
| `last_move_dir[N]` | `uint8_t` | N | Direction sensors | Movement resolution kernel |
| `challenge_bits[N]` | `uint32_t` | 4N | Challenge eval | Challenge eval kernel |
| `rng_state[N]` | `uint64_t` | 8N | RANDOM sensor, MOVE_RANDOM, any stochastic op | Every kernel that draws RNG |
| `genome_fingerprint[N]` | `uint64_t` | 8N | GENETIC_SIM_FWD sensor | Generation spawn |

**Total fixed per-agent fixed-size footprint:** ~40 bytes/agent, plus genome (Section 6) and neural net (Section 7). For `N = 4096`: ~160 KB plus genome/nnet, trivially fitting in global memory.

### 5.2 Why widen `age` to `uint16_t`?

Current code uses `unsigned`. Agents never live longer than `stepsPerGeneration` (typically < 1000). `uint16_t` suffices, halves the bandwidth for AGE sensor loads, and aligns naturally with `osc_period`.

### 5.3 Why `uint64_t` for RNG state?

A full 64-bit xorshift64 state gives a period of `2^64 - 1`, which is more than adequate for simulation purposes. Smaller generators (like 32-bit xorshift32) have shorter periods and weaker statistical properties. 8 bytes per agent × 4096 agents = 32 KB — negligible.

### 5.4 Why a `genome_fingerprint`?

Precomputed at generation spawn time from the agent's genome. Replaces the pointer-chased, variable-length genome comparison of `GENETIC_SIM_FWD` with a single 8-byte load plus a popcount. Detailed in Section 11.

### 5.5 What disappears

- `Indiv::index`: identical to the array index. No storage needed.
- `Indiv::genome` (as a member): replaced by the global genome SoA buffers (Section 6).
- `Indiv::nnet` (as a member): replaced by the global neural-net SoA buffers (Section 7).

---

## 6. Genome Storage

### 6.1 Layout: padded, transposed SoA

```cpp
#define GENOME_MAX_LENGTH  P_genomeMaxLength   // compile-time or kernel constant

// Two parallel buffers (split of Gene):
__global uint16_t genome_conn[GENOME_MAX_LENGTH * N];  // [gene_slot][agent]
__global int16_t  genome_wgt [GENOME_MAX_LENGTH * N];  // [gene_slot][agent]

// Per-agent length:
__global uint8_t  genome_length[N];  // actual number of valid genes in [0 .. GENOME_MAX_LENGTH]
```

Index of gene slot `j` of agent `i`: `j * N + i`.

### 6.2 Why transposed

When the feedForward kernel processes all agents in parallel and each kernel instance walks through its genome from gene 0 to gene `length-1`:

- **At iteration `j = 0`:** every work-item reads `genome_conn[0 * N + i]` for `i = 0..N-1`. Work-items with adjacent `i` read adjacent addresses. **Coalesced.**
- **At iteration `j = 1`:** every work-item reads `genome_conn[1 * N + i]`. Same pattern. **Coalesced.**

The access pattern is perfect because "all work-items advance to gene j together" matches the transposed layout exactly.

If we had stored `genome_conn[i * MAX + j]` (non-transposed), work-item `i`'s access to gene `j` would be at offset `i * MAX + j`. Two adjacent work-items would be at offsets `i*MAX+j` and `(i+1)*MAX+j` — stride of `MAX × 2` bytes. Catastrophically uncoalesced.

### 6.3 Handling variable length

Two mechanisms combined:

- **Per-agent length** (`genome_length[i]`): the kernel loop is `for (j = 0; j < genome_length[i]; ++j)`.
- **Warp coherence via sorting:** once per generation, after reproduction, the host sorts agents by descending `genome_length`. Agents with similar lengths end up with adjacent indices, so a wavefront of 32 or 64 adjacent work-items has nearly-uniform loop bounds. Divergence within a wavefront is reduced to the worst-case spread among any 32 adjacent agents, which is small if the sort is tight.

**Alternative considered:** fully uniform loop `for (j = 0; j < GENOME_MAX_LENGTH; ++j)` with a predicate that no-ops past `genome_length[i]`. This kills divergence entirely but forces every agent to pay the full cost of the maximum genome. The sort-based approach is cheaper when genome length distribution has high variance, the uniform loop is cheaper when the distribution is tight. We adopt the sort approach and revisit in Step 2.

### 6.4 Why padding is acceptable

Worst case: `GENOME_MAX_LENGTH = 256`, `N = 4096`, 4 bytes per gene (2 for connectivity + 2 for weight). Total: 256 × 4096 × 4 = 4 MiB. Trivial. Padding waste depends on genome length distribution; even 50 % waste is 2 MiB excess — negligible.

### 6.5 Opportunities unlocked

- **Constant, coalesced bandwidth per gene slot.** Processing gene `j` across all agents is one 128-byte (or 256-byte) transaction per wavefront.
- **Hardware prefetching works.** The linear sweep `j = 0, 1, 2, ...` is the exact pattern GPU memory controllers are optimized for.
- **Genome mutation at the generation boundary** can itself be a kernel (deferred; host-side at first).

---

## 7. Neural Network Storage

Two sub-structures to store per agent: the compiled connection list and the per-neuron running output.

### 7.1 Connections — padded, transposed SoA

Identical layout to the genome. The compiled connections after culling are stored as gene-equivalent entries, up to `MAX_CONNECTIONS` per agent.

```cpp
#define MAX_CONNECTIONS  (2 * GENOME_MAX_LENGTH)   // culling may preserve or add

__global uint16_t conn_packed[MAX_CONNECTIONS * N];
__global int16_t  conn_weight[MAX_CONNECTIONS * N];
__global uint8_t  conn_length[N];
```

`conn_packed` encodes source type (1 bit), source index (7 bits), sink type (1 bit), sink index (7 bits) — same 16-bit layout as the genome's `conn` field.

**Why separate from genome:** the compiled network is shorter in general than the genome (useless neurons are pruned) and is produced by a culling function at generation spawn. Genome and nnet are written once per generation, read every simStep.

### 7.2 Connection ordering invariant

Inside each agent's connection list, connections whose sink is a **neuron** are placed first, connections whose sink is an **action** are placed second. This is the same invariant as in the current code and is essential for the single-pass feedForward:

- Phase 2 of feedForward scans connections in order. Neuron-sink connections accumulate into a per-neuron local accumulator. Action-sink connections accumulate directly into an action sum.
- The read of a neuron's output (when it is a source) always returns the value from the *previous* simStep, because Phase 3 (applying tanh and writing the new outputs) has not yet happened.

**Critical point for the GPU:** the accumulator must live in **private memory** (registers / stack frame of the work-item), not in global memory. If all 32 work-items in a wavefront have up to `MAX_NEURONS` accumulators in private memory, at `MAX_NEURONS = 32` and 4 bytes each this is 128 bytes per work-item = 4 KiB per wavefront. That is well within register capacity.

### 7.3 Neuron outputs — ping-pong? not needed

The current algorithm reads all neuron outputs from the "previous simStep" values, accumulates into a local (non-neuron-array) accumulator, then writes the new outputs into the neuron array. Because the accumulator is separate, **there is no intra-step read-after-write hazard** on the neuron array itself. This is preserved exactly.

Therefore a single buffer `float neuron_output[MAX_NEURONS * N]` suffices. Thread `i` reads `neuron_output[k * N + i]` (transposed) for every active neuron `k`, accumulates in private registers, then at the end of the kernel writes the new `neuron_output[k * N + i]`.

A ping-pong (two buffers swapped each step) is an optional safety net. It is not required by the algorithm.

### 7.4 Driven flag

```cpp
__global uint8_t neuron_driven[MAX_NEURONS * N];   // 1 if driven, 0 otherwise
__global uint8_t neuron_count [N];                 // actual number of neurons for this agent
```

Written once per generation by the wiring kernel (or host). Read every simStep to decide whether to apply tanh or set the undriven-default of 0.5.

### 7.5 Opportunities unlocked

- The entire feedForward inner loop is a sequence of coalesced loads into private accumulators — the GPU's favorite pattern.
- The tanh application is a pure per-work-item arithmetic pass, perfectly parallel with no memory pressure.

---

## 8. Grid Representation

### 8.1 Layout: flat 2D buffer

```cpp
__global uint16_t grid[SIZE_X * SIZE_Y];   // row-major: grid[y * SIZE_X + x]
```

Cell encoding unchanged: 0 = empty, 0xFFFF = barrier, otherwise an agent index.

**Row-major vs column-major:** the current CPU code is column-major (`data[x][y]`). Row-major is more standard in GPU/image contexts and aligns with `image2d_t` coordinate conventions. Either choice is fine for performance as long as the neighborhood scans and sensor lookups use the matching indexing. We pick **row-major**.

### 8.2 Read-only access via `image2d_t` (recommendation)

During the parallel phase, the grid is read-only. Binding it as an `image2d_t` enables the dedicated texture cache, which is optimized for 2D spatial access patterns. A neighborhood scan of radius 4 (e.g., the POPULATION sensor) reads roughly 49 cells in a disc; through `image2d_t` most of these hit the texture cache after the first few reads.

```cpp
__read_only image2d_t grid_img;   // in sensor/feedForward kernels

// In a write kernel (movement resolution), we use the plain __global buffer.
```

OpenCL does not let a kernel treat the same buffer as both writable `__global` and readable `image2d_t` simultaneously. The split is acceptable: we have separate kernels for sensor/feedForward (read) and movement resolution (write), with a kernel boundary between them serving as the synchronization point.

### 8.3 Barriers

```cpp
__constant Coord barrier_locations[MAX_BARRIERS];
__constant uint32_t barrier_count;
```

Barriers are written once at generation start, never modified during simStep. `__constant` memory is a small (typically 64 KB) cached region ideal for small tables that every work-item reads.

Barriers are also redundantly encoded in the grid (value 0xFFFF), so most barrier queries can be answered by a single grid read. The explicit list is retained only for the few operations that iterate barriers directly.

### 8.4 Opportunities unlocked

- `image2d_t` neighborhood reads are hardware-accelerated and nearly free compared to naked `__global` reads for the same cells.
- A flat 1D-indexed grid removes the nested `std::vector` indirection and gives pointer-free O(1) access from any kernel.

---

## 9. Signal Representation

### 9.1 Layout: flat 3D buffer, `uint32_t` per cell

```cpp
__global uint32_t signal[LAYERS * SIZE_X * SIZE_Y];
// signal[layer * SIZE_X * SIZE_Y + y * SIZE_X + x]
```

**Why `uint32_t` and not `uint8_t`?** Because atomic operations on 8-bit values are not portably supported by OpenCL. Promoting to 32 bits trades memory (4×) for simple, portable `atomic_add`. For `LAYERS = 1`, `SIZE_X = SIZE_Y = 128`: `1 × 128 × 128 × 4 = 64 KiB`. Trivial.

The value range semantically stays 0–255 (clamped on write). The upper 24 bits are unused.

### 9.2 Emit signal — atomic_add with local-memory staging

The current `increment` adds +2 to the center cell and +1 to up to 7 neighbors — up to 8 cells per emit. On a GPU with many simultaneous emitters, naive global atomics on the same neighborhood would serialize heavily.

**Two-stage strategy:**

1. **Stage 1 (local memory):** if a work-group's agents are spatially clustered, allocate a small `__local` tile of signal increments covering the work-group's bounding box (or a fixed neighborhood around the group). Agents atomically add to the local tile.
2. **Stage 2 (global atomic flush):** at the end of the kernel, one work-item per tile cell atomically adds the accumulated delta to the global signal buffer. This reduces global atomic count by roughly the work-group size.

Stage 1 only helps when agents in the same work-group are spatially close. If work-groups are assigned by agent index (not position), proximity is not guaranteed. **Design lever:** periodically (e.g., at generation start, or every few simSteps) sort agents by spatial position so adjacent agent indices have adjacent positions. Then the local-memory staging is effective.

If this optimization proves too complex for the first cut, fall back to direct global atomics. Contention is likely tolerable unless emit rate is very high.

### 9.3 Signal fade — a full-grid kernel

```
kernel signal_fade:
    per-cell:  signal[c] = max(0, signal[c] - FADE_AMOUNT)
```

Fully embarrassingly parallel. One work-item per cell, or one work-item per 4 cells (vectorized). Launched once per simStep after the emit phase has completed (the kernel boundary is the barrier).

### 9.4 Signal read — coalesced or image-cached

Reads during sensor evaluation go through the same `image2d_t`-or-`__global` choice as the grid. For signal reads with `read_only` access, we can re-bind the signal buffer as an `image2d_t` for cached access.

### 9.5 Opportunities unlocked

- The `uint32_t` promotion is a simpler and more portable alternative to 8-bit atomic dances with negligible memory cost.
- Local-memory staging is a classic GPU optimization that directly applies here and can be tuned in Step 2.

---

## 10. Queue-Free Conflict Resolution

### 10.1 Move conflicts

The current CPU model: all agents queue their desired move, then a sequential drain resolves conflicts (first-come-first-served). On the GPU, we flip this to **parallel atomic claim** on the destination grid cell:

```c
// pseudo-OpenCL (simplified)
int i = get_global_id(0);
if (!alive[i]) return;

int16_t tx = desired_x[i];   // computed in feedForward kernel
int16_t ty = desired_y[i];
uint16_t self = i + 1;       // agent id in grid cell

int cell_idx = ty * SIZE_X + tx;

// Try to atomically replace an empty cell (0) with self's id.
uint16_t old = atomic_cmpxchg(&grid[cell_idx], 0u, self);
if (old == 0) {
    // We won the cell. Clear our old cell and update our position.
    int old_cell_idx = loc_y[i] * SIZE_X + loc_x[i];
    atomic_xchg(&grid[old_cell_idx], 0u);
    loc_x[i] = tx;
    loc_y[i] = ty;
    last_move_dir[i] = direction_from(dx, dy);
}
// else: lost the race, move silently fails. Same semantics as the CPU version.
```

**What we lose:** Reproducibility. Which agent wins a contested cell depends on which wavefront schedules first on the hardware — non-deterministic.

**What we gain:** No serial drain, no host-side critical section, all movement resolved in parallel.

**Subtle correctness issue:** Two agents A and B might both leave cell X and race into cell X from opposite sides while a third agent C tries to enter X. The atomic_cmpxchg prevents double occupation, but the order of "clear my old cell" and "claim my new cell" matters. If A clears its old cell after B tried to enter that cell, B sees it as occupied and fails — correct. If A clears its old cell before B arrives, B might succeed in entering — also correct. No two agents ever occupy the same cell because of the cmpxchg.

One residual hazard: after A has claimed its new cell but before A clears its old cell, can another agent D (who hadn't moved yet) be processed by the neural net pass reading the grid? No — movement resolution is in a **separate kernel** from feedForward. Within movement resolution, all reads are of the pre-movement state and all writes commit atomically. The kernel boundary after movement resolution is the point at which the grid is fully consistent.

Another hazard: two agents both want to leave cell X simultaneously (because two agents were somehow recorded at X — shouldn't happen but belt-and-suspenders). The `atomic_xchg(&grid[old_cell], 0)` is idempotent; whoever executes it last leaves the cell at 0, which is correct.

### 10.2 Death conflicts

`KILL_FORWARD`:

```c
int target = grid[forward_cell_idx];
if (target == 0 || target == 0xFFFF) return;
int target_agent = target - 1;
atomic_or(&alive[target_agent], 0);   // actually atomic_min to 0, see below
alive[target_agent] = 0;               // a plain write suffices
```

Because `alive` is written only to 0 (never set back to 1 during the parallel phase), a plain write is **idempotent**: multiple killers of the same target produce the correct result without atomics. No queue needed.

A cleanup pass then zeroes the dead agents' grid cells:

```c
// In a subsequent kernel, each work-item checks its own alive flag.
if (!alive[i]) {
    int cell = loc_y[i] * SIZE_X + loc_x[i];
    atomic_cmpxchg(&grid[cell], (uint16_t)(i + 1), 0u);
    // CAS: only clear if the cell still references this agent.
    // Another agent might have moved into it if we already cleared at movement time.
}
```

### 10.3 Self-field writes during parallel phase

`SET_RESPONSIVENESS`, `SET_OSCILLATOR_PERIOD`, `SET_LONGPROBE_DIST`, `age++`: each work-item writes only its own agent's field. Guaranteed no aliasing because we bind `work-item i ↔ agent i`. No atomics needed, no queues.

### 10.4 Summary

Every queue in the current design is eliminated:

| Original queue | Replacement |
|---|---|
| `moveQueue` | `atomic_cmpxchg` on grid cells in a movement resolution kernel |
| `deathQueue` | Idempotent write to `alive[target]` in the action kernel |
| Signal `increment` critical section | Global `atomic_add` (optionally staged through local memory) |

**Reproducibility cost:** Move contests are now non-deterministic. Everything else is preserved.

---

## 11. Sensor Catalogue — GPU-Adapted

Re-categorized by their new GPU cost profile after the layout changes above.

### 11.1 Group A — Pure per-agent arithmetic (unchanged)

`LOC_X`, `LOC_Y`, `BOUNDARY_DIST_X`, `BOUNDARY_DIST_Y`, `BOUNDARY_DIST`, `LAST_MOVE_DIR_X`, `LAST_MOVE_DIR_Y`, `OSC1`, `AGE`, `RANDOM`.

Each reads at most a couple of SoA fields of the agent itself. All coalesced, all no-divergence. These sensors are essentially free on the GPU.

### 11.2 Group B — Grid neighborhood reads (cheaper via `image2d_t`)

`POPULATION`, `POPULATION_FWD`, `POPULATION_LR`, `BARRIER_FWD`, `BARRIER_LR`, `LONGPROBE_POP_FWD`, `LONGPROBE_BAR_FWD`.

These iterate a small neighborhood of grid cells. With `image2d_t` binding of the grid, the texture cache absorbs most of the cost after the first few reads per wavefront (spatial locality among work-items in the same wavefront helps — if they're spatially close, their neighborhoods overlap).

`LONGPROBE_*` sensors have early-exit loops (stop on first occupied cell). This causes divergence within a wavefront (some lanes stop at distance 3, others at distance 15). Acceptable for now; revisit only if profiling shows it to be a hotspot.

### 11.3 Group C — Signal reads (cheaper via `image2d_t`)

`SIGNAL0`, `SIGNAL0_FWD`, `SIGNAL0_LR`.

Same treatment as the grid: read-only during sensor phase, bind as `image2d_t` for the sensor/feedForward kernel.

### 11.4 Group D — `GENETIC_SIM_FWD` — **replaced by fingerprint**

**Original behavior:** read the forward neighbor's full genome (variable length, heap-allocated), compare gene-by-gene against own genome, return similarity score.

**Problems on GPU:** two-level pointer chase (grid → agent → genome), variable-length loop causing warp divergence, cross-agent read that forces non-local memory access.

**Replacement:** compute a 64-bit fingerprint of each agent's genome once per generation. The fingerprint is a hash that preserves similarity: similar genomes produce fingerprints with many bits in common; dissimilar genomes produce fingerprints that differ in roughly half their bits (random-looking).

**Similarity metric:** Hamming distance between the two fingerprints, normalized:
```
similarity = 1.0 - popcount(fingerprint_self ^ fingerprint_neighbor) / 64.0
```

Cost on the GPU:
- 1 grid cell read (the forward cell).
- If empty or barrier: return 0.0.
- Else: 1 load of `genome_fingerprint[neighbor_idx]` — perfectly coalesced when all work-items have adjacent neighbor indices (not guaranteed, but often true when agents cluster spatially).
- `popcount` and `xor` are single-cycle GPU instructions.

**Behavioral difference:** the fingerprint is an approximation. Two genomes can share no genes yet have a fingerprint similarity of ~0.5 by chance. The evolutionary dynamics change slightly. This is a **deliberate behavioral simplification** accepted in the premise of the refactoring.

**Fingerprint construction:** deferred to Step 2. Candidates include SimHash, MinHash compressed to 64 bits, or a locality-sensitive hash over gene tuples. Choice affects the correlation between true genome similarity and fingerprint similarity.

### 11.5 Opportunities unlocked

- The worst-divergence, worst-locality sensor in the catalogue becomes the cheapest cross-agent sensor.
- Spatial-sort of agents (Section 9.2) improves coalescing on the fingerprint load.
- Fingerprint itself is a cheap operation during reproduction and can even be moved to the host with no issue.

---

## 12. Action Catalogue — GPU-Adapted

### 12.1 Self-writes: unchanged pattern, no queue

`SET_RESPONSIVENESS`, `SET_OSCILLATOR_PERIOD`, `SET_LONGPROBE_DIST`: each work-item writes only to its own agent's SoA cell. Plain writes, no atomics, no queues. Execution kernel writes `responsiveness[i] = ...`.

### 12.2 Movement actions: produce a desired destination, resolve in a later kernel

Each movement action (`MOVE_X`, `MOVE_Y`, `MOVE_FORWARD`, `MOVE_*`) contributes to a pair of accumulators in private memory: `dx_sum`, `dy_sum`. At the end of the action pass, the work-item writes:

```c
desired_x[i] = clamp(loc_x[i] + sign(dx_sum) * prob_step(dx_sum), 0, SIZE_X - 1);
desired_y[i] = clamp(loc_y[i] + sign(dy_sum) * prob_step(dy_sum), 0, SIZE_Y - 1);
```

where `prob_step` is the probabilistic move decision (threshold tanh like the original). These two buffers (`desired_x`, `desired_y`) are **new** SoA buffers, temporary and recomputed every simStep.

A subsequent movement resolution kernel then applies the atomic-CAS pattern of Section 10.1.

### 12.3 `EMIT_SIGNAL0`: direct atomic writes

In the action execution kernel, if the signal threshold is crossed, the work-item performs up to 8 `atomic_add` operations on the signal buffer (one for the center cell, up to 7 for the neighborhood). With the `uint32_t` promotion (Section 9.1), this is portable.

Local-memory staging (Section 9.2) is a later optimization, not required for the first kernel cut.

### 12.4 `KILL_FORWARD`: idempotent write to target's `alive` flag

As described in Section 10.2.

### 12.5 Action execution order

Current CPU code has an implicit order from action index 0 to `NUM_ACTIONS - 1`. Within a single work-item this order is preserved by the sequential code in the kernel. No cross-agent ordering is defined, nor needed.

---

## 13. Simulation Loop — Kernel Breakdown

### 13.1 Kernel pipeline per simStep

```
for simStep in 0..stepsPerGeneration:

    // ALL KERNELS launched on the full population (or full grid for per-cell ones)
    
    K1: feedForward_and_actions
        inputs  (read): alive, loc_*, age, osc_period, last_move_dir,
                         responsiveness, long_probe_dist,
                         genome fingerprint (if GENETIC_SIM_FWD active),
                         conn_packed, conn_weight, conn_length,
                         neuron_output, neuron_driven, neuron_count,
                         grid (image2d_t), signal (image2d_t),
                         rng_state
        outputs (write): neuron_output (new values),
                          responsiveness, osc_period, long_probe_dist (self-updates),
                          age (self-increment),
                          desired_x, desired_y (new, transient buffers),
                          alive (KILL_FORWARD targets — idempotent writes),
                          rng_state
        atomic ops:       on signal buffer (for EMIT_SIGNAL0 — uint32_t atomic_add)
        launch size:      N (one work-item per agent)
    
    // Kernel boundary = global barrier. Everything above has committed.
    
    K2: movement_resolution
        inputs  (read): alive, loc_*, desired_x, desired_y
        outputs (write): loc_*, last_move_dir, grid (atomic_cmpxchg)
        launch size:     N
    
    // Kernel boundary.
    
    K3: post_death_grid_cleanup
        inputs  (read): alive, loc_*, grid
        outputs (write): grid (atomic_cmpxchg, only dead agents' former cells)
        launch size:     N
        [only need to run when at least one KILL_FORWARD fired this step — can
         be gated by a flag set in K1, or always run unconditionally.]
    
    // Kernel boundary.
    
    K4: signal_fade
        inputs  (read): signal
        outputs (write): signal
        launch size:    LAYERS * SIZE_X * SIZE_Y (one work-item per cell)
    
    // Kernel boundary.
    
    K5: challenge_eval (optional, simple challenges only)
        inputs  (read): alive, loc_*, birth_*, challenge_bits
        outputs (write): challenge_bits, alive (some challenges kill early)
        launch size:     N
        [complex relational challenges may still run on host — see Section 14.2]
```

**Total: 5 kernel launches per simStep.** Each launch has a fixed per-launch overhead (roughly 5–20 µs depending on driver). With `stepsPerGeneration = 500`, that's 2,500–10,000 launches per generation — manageable but not free. Step 2 can consider fusing K2+K3 or K1+K2 if launch overhead becomes a measurable cost.

### 13.2 What the host still does per simStep

Ideally: **nothing.** All five kernels are submitted back-to-back into the same command queue with implicit ordering. The host does not block between them. The host only blocks at the generation boundary (Section 14) to read back survivor data.

If a challenge is so complex that it cannot be expressed as a per-agent kernel, that single step may require a host round-trip. This is the exception, not the rule.

### 13.3 Opportunities unlocked

- No per-simStep host work means no PCIe traffic for 500 simSteps.
- Kernel boundaries are the only synchronization needed, and they are free (the driver handles them).
- Each kernel has a coherent, focused job — easier to profile and optimize individually.

---

## 14. Generation Boundary and Reproduction

### 14.1 What happens at the generation boundary

At the end of generation `g`:

1. **Read back to host:** `alive[N]`, survivor genomes (the `genome_conn` and `genome_wgt` buffers for living agents), `genome_length[N]`, and any per-agent statistics needed for selection.
2. **Host-side work:**
   - Evaluate survivors (challenge-based fitness).
   - Pair survivors and produce child genomes (asexual or sexual crossover).
   - Apply mutation operators to child genomes.
   - Compile each child genome into a neural network (the culling step).
   - Compute each child's genome fingerprint.
   - Decide spawn locations (random or challenge-specified).
3. **Write to device:** full new population state — genomes, neural nets, fingerprints, initial positions, RNG seeds.

### 14.2 Why keep the boundary on the host (initially)

- The operations are irregular: variable-length copying of genes, mutation that may change genome length, culling that builds a graph and topologically sorts it.
- The work is comparable in cost to a handful of simSteps. Migrating it later is an optimization, not a correctness issue.
- Host-side code can reuse the existing, tested logic from `genome.cpp` and `spawnNewGeneration.cpp` almost unchanged.

### 14.3 Could the boundary move to the GPU?

Yes, in principle:
- Mutation and crossover can be expressed per-child with a capped genome length.
- Culling is a per-agent graph operation that fits in private memory if the graph is small.
- Fingerprint computation is trivially parallel.

We leave this as a later optimization. It's realistic, but it adds complexity without changing the first-cut behavior.

### 14.4 Data transfer sizing

For `N = 4096`, `GENOME_MAX_LENGTH = 256`, 4 bytes per gene: 4 MiB per direction per generation. Over PCIe Gen4 (~16 GB/s effective), that's ~0.5 ms per direction. Negligible compared to a generation's worth of simSteps.

### 14.5 Opportunities unlocked

- Keeping the boundary on the host means we can implement the GPU refactoring **without rewriting reproduction**. Huge de-risking of the project.
- Clean transfer interface (one upload + one download per generation) makes profiling the GPU side trivially isolated.

---

## 15. Feature Changes Summary

| Feature | Change | Behavioral impact | Performance impact |
|---|---|---|---|
| Per-agent struct `Indiv` | Decomposed into flat SoA buffers | None | Large positive (coalescing) |
| Genome storage | Padded, transposed SoA; fixed capacity | None | Large positive (coalescing, no pointer chase) |
| Neural net storage | Padded, transposed SoA; fixed capacity | None | Large positive |
| Grid | Flat 2D, optionally `image2d_t` for reads | None | Positive (cache) |
| Signals | Promoted `uint8_t` → `uint32_t`; `atomic_add` | None (value semantics unchanged) | Slight memory cost; eliminates portability pain |
| `GENETIC_SIM_FWD` | Replaced with 64-bit fingerprint similarity | Approximation, slight evolutionary drift | Large positive (no cross-agent pointer chase) |
| Move queue | Atomic CAS on grid cells | **Loss of reproducibility** | Large positive (parallel resolution) |
| Death queue | Idempotent writes to `alive[]` | None beyond move queue loss of determinism | Large positive |
| Challenge evaluation | Simple challenges parallelized as kernels; complex relational challenges may stay on host | None | Minor positive to neutral |
| RNG | Per-agent xorshift64 in SoA | Different RNG sequence per agent | None (possibly positive vs global RNG) |
| Reproduction | Unchanged, stays on host | None | None |
| Per-agent sorting by genome length | New step at generation boundary | None (agent index reshuffling invisible to the simulation) | Positive (warp coherence) |
| Per-agent sorting by spatial position (optional) | New step each N simSteps | None | Positive (signal emit locality, fingerprint locality) |

---

## 16. Memory Budget Estimate

For a typical configuration `N = 4096`, `SIZE_X = SIZE_Y = 128`, `LAYERS = 1`, `GENOME_MAX_LENGTH = 256`, `MAX_NEURONS = 32`, `MAX_CONNECTIONS = 512`:

| Buffer | Size |
|---|---|
| Per-agent fixed fields (~40 B × N) | 160 KiB |
| Genome (4 B × 256 × N) | 4 MiB |
| Neural net connections (4 B × 512 × N) | 8 MiB |
| Neuron outputs (4 B × 32 × N) | 512 KiB |
| Neuron driven flags (1 B × 32 × N) | 128 KiB |
| Grid | 32 KiB |
| Signal | 64 KiB |
| Transient desired_x / desired_y | 16 KiB |
| Total | ~13 MiB |

Fits comfortably in any modern discrete GPU's DRAM. Even scaling `N` to 65,536 only multiplies by 16, yielding ~200 MiB — still trivial on a 4 GiB+ GPU.

---

## 17. Deferred Decisions for Step 2

Each of the following is a discrete topic we will tackle one at a time in follow-up conversations. They correspond to the pitfalls from Section 14 of the source document, re-framed against the new architecture.

1. **Genome length distribution vs. warp coherence.** Quantify the divergence cost of the sort-then-iterate strategy vs. the uniform-loop-with-predicate strategy. Decide under what parameters each wins.

2. **`GENETIC_SIM_FWD` fingerprint choice.** SimHash, MinHash-to-64, random-projection LSH, or a handcrafted Hamming-friendly hash. Evaluate the correlation between fingerprint similarity and true genome similarity under typical mutation patterns.

3. **Movement resolution — atomic_cmpxchg strategy.** Confirm the correctness of the two-step (claim new, clear old) pattern under all concurrent scenarios. Consider alternatives (sort-based, two-pass with conflict detection).

4. **Signal emit — local memory staging.** Quantify the reduction in global atomic contention achievable with per-work-group tiling. Decide work-group size and tile geometry.

5. **Grid and signal `image2d_t` binding.** Confirm driver support, evaluate the coalescing / cache benefit on realistic neighborhood patterns, decide whether both read-only bindings are worth the extra kernel variants.

6. **RNG choice and seeding.** xorshift64, PCG, Philox. Per-agent state management across generations. Reproducibility-for-testing vs. performance.

7. **Kernel fusion.** Whether K1+K2 or K2+K3 should be merged, given launch overhead numbers on the target hardware.

8. **Sort strategies.** By genome length, by spatial position, hybrid — frequency of re-sort, the sort algorithm (radix, bitonic, thrust), and the index-remapping bookkeeping.

9. **Challenge migration.** Which challenges move cleanly to GPU, which need host-side handling, and how the challenge-specific `challenge_bits` field is populated.

10. **Host-device data transfer at generation boundary.** Pinned host buffers, async transfers overlapping with reproduction compute, persistent device-side storage layouts.

11. **Per-simStep host flow.** Command queue construction, profiling events, debugging without per-step readbacks.

---

**End of Step 1 design document.** Next, pick any constraint from Section 17 (or another one I may have missed) and we will dig into it in detail.
