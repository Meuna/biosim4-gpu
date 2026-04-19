# GPU / OpenCL Terminology Primer

**Purpose:** Shared glossary of GPU concepts used across all design documents.

## Table of Contents

1. [Execution model: work-items, work-groups, wavefronts](#work-items-work-groups-wavefronts)
2. [Warp divergence and coherence](#warp-divergence)
3. [Memory coalescing](#memory-coalescing)
4. [Pointer chasing](#pointer-chasing)
5. [Memory locality — spatial and temporal](#memory-locality)
6. [AoS, SoA, and AoSoA](#aos-soa-aosoа)
7. [Atomics and contention](#atomics-and-contention)
8. [Local memory and barriers](#local-memory-and-barriers)
9. [Image objects (texture memory)](#image-objects)
10. [Host-device transfer](#host-device-transfer)

## Work-items, work-groups, wavefronts {#work-items-work-groups-wavefronts}

OpenCL launches a kernel across an **NDRange**: a 1D, 2D, or 3D grid of
**work-items**. A work-item is the GPU equivalent of a thread. Each work-item
runs the same kernel code but typically with a different index
(`get_global_id(0)`), and therefore typically touches a different slice of
memory.

Work-items are clustered into **work-groups**. All work-items in the same
work-group run on the same compute unit (SM in NVIDIA terms, CU in AMD terms).
Only within a work-group is cheap synchronization available:
`barrier(CLK_LOCAL_MEM_FENCE)` waits for every work-item in the group to reach
that line.

Inside a work-group, the hardware further clusters work-items into
**wavefronts** (AMD term) or **warps** (NVIDIA term). Wavefronts are typically
32 (NVIDIA), 32 or 64 (AMD), 8/16/32 (Intel). A wavefront executes **in
lockstep**: all lanes execute the same instruction at the same clock cycle.
This is called **SIMT** (Single Instruction, Multiple Threads).

**Why this matters:** The wavefront is the atomic unit of GPU execution. The
entire wavefront pays the cost of its slowest lane. If 31 lanes have finished
and 1 lane is still running a loop, the other 31 sit idle but still consume
execution slots.

## Warp divergence and coherence {#warp-divergence}

**Warp divergence** occurs when work-items within the same wavefront take
different code paths. Because all lanes must execute the same instruction,
divergence is handled by **predication**: the hardware masks off the lanes that
don't want to execute the current branch, runs the `if` branch with only the
"true" lanes active, then runs the `else` branch with only the "false" lanes
active. Both branches run serially. The cost is roughly additive.

Concretely:
- `if (x > 0) { work_A() } else { work_B() }` — if half the lanes take each
  branch, both `work_A` and `work_B` execute on the full wavefront, but with
  half the lanes masked during each. Effective throughput is halved.
- `for (i = 0; i < agent_specific_length; ++i) { ... }` — the wavefront's loop
  runs for as many iterations as the *longest* lane needs. Short lanes sit idle
  for the remaining iterations.

**Warp coherence** is the opposite: all lanes take the same path and execute
the same number of iterations. The full throughput of the wavefront is realized.

**Design lever:** Any time variable-length iteration appears per-agent, we can
buy back coherence by *sorting* agents so that similar-length agents end up in
the same wavefront. Adjacent thread indices land in the same wavefront, so if
agents with similar genome length have adjacent indices, they diverge less.

## Memory coalescing {#memory-coalescing}

When a wavefront issues a memory load, the hardware does not issue 32 separate
transactions. It looks at the 32 addresses and groups them into **memory
transactions** of 32, 64, or 128 bytes aligned on the natural boundary. If all
32 addresses fall into a single 128-byte aligned segment, it is **one**
transaction — this is **coalesced access**.

- Lane 0 reads `buf[0]`, lane 1 reads `buf[1]`, …, lane 31 reads `buf[31]`
  (4-byte floats) → 128 bytes contiguous → 1 transaction. Perfectly coalesced.
- Lane 0 reads `buf[0]`, lane 1 reads `buf[1024]`, lane 2 reads `buf[2048]`, …
  → 32 scattered transactions. Effective bandwidth divided by 32.

**Design lever:** Lay out data so that *adjacent work-items read adjacent
addresses* when they read "the same logical field". If work-item `i` processes
agent `i`, then `agent[i].age` must live at address `base_age + i *
sizeof(uint32_t)` (SoA), **not** at `base + i * sizeof(Indiv) + offset_of_age`
(AoS with wide stride).

## Pointer chasing {#pointer-chasing}

A **pointer-chased** access pattern means following a pointer to reach data.
Example: to read an agent's first gene from the CPU model, we have
`Indiv → genome.data() pointer → heap block → gene[0]`. The CPU can often hide
this behind prefetchers and deep caches. The GPU cannot: each pointer
dereference is a full memory round-trip (~400–800 cycles on GDDR6, much worse
than arithmetic).

Moreover, pointer-chased accesses are **never coalesced**: adjacent work-items
that dereference their own pointer end up reading 32 scattered heap locations,
blowing the memory subsystem.

**Design lever:** Flatten everything. No nested `std::vector`, no per-agent heap
blocks. One big contiguous buffer per logical field, indexed by agent ID.

## Memory locality — spatial and temporal {#memory-locality}

- **Spatial locality:** After touching address `A`, a nearby address (`A + 1`,
  `A + 64`) is cheap because it likely came in the same cache line or memory
  transaction.
- **Temporal locality:** After touching address `A`, touching it again soon is
  cheap because it's still in cache.

GPU caches are small per compute unit (L1 of tens of KB, L2 of a few MB
shared). Relying on temporal locality is weaker than on a CPU. Spatial
locality, however, is *the* primary optimization target because it drives
coalescing.

## AoS, SoA, and AoSoA {#aos-soa-aosoа}

- **AoS (Array of Structures):** `struct Indiv { uint32_t age; Coord loc; ... }`
  then `Indiv indivs[N]`. Reading `indivs[i].age` for all `i` is strided by
  `sizeof(Indiv)` — anti-coalesced. Cache-friendly on the CPU, toxic on the GPU.
- **SoA (Structure of Arrays):** `struct Population { uint32_t age[N]; int16_t
  loc_x[N]; int16_t loc_y[N]; ... }`. Reading `age[i]` for all `i` is
  contiguous — perfectly coalesced. This is the GPU standard.
- **AoSoA (hybrid):** Group fields into small structs but block the arrays by
  wavefront size. Rarely worth the complexity unless a kernel consistently
  co-reads several fields.

The biosim4-gpu design uses **pure SoA** for all per-agent state. AoSoA is
noted as a possible later optimization but is not adopted upfront.

## Atomics and contention {#atomics-and-contention}

**Atomic operations** (`atomic_add`, `atomic_cmpxchg`, `atomic_max`, …) let
multiple work-items modify a shared location with a consistent
read-modify-write sequence. They are essential for cases like "many agents want
to write to the same signal cell".

Costs:
- An uncontended atomic is roughly as expensive as a normal global memory write.
- A **contended** atomic (many work-items hitting the same address in the same
  cycle) serializes. Contention of 32 lanes on one address roughly costs 32× an
  uncontended write.
- Byte-wide (`uint8_t`) atomics are **not universally supported**. Portable code
  uses 32-bit atomics. If the value naturally fits in a byte, either promote it
  to 32 bits or do a packed CAS loop over a 4-byte word containing the byte.
  Promotion is simpler.

**Design lever:** Where we must write contended data (signals, shared counters),
use 32-bit atomics. Where possible, reduce contention by aggregating writes in
**local memory** first, then flushing to global memory.

## Local memory and barriers {#local-memory-and-barriers}

**Local memory** (`__local` in OpenCL) is a small, fast, per-work-group
scratchpad, typically 16–64 KB. Access latency is close to a register. All
work-items in the same work-group share it. It is the ideal place to:
- Stage reductions before writing to global memory.
- Cache read-only data that every work-item in the group will read.
- Build per-group histograms or atomic counters that are then flushed once to
  global memory.

`barrier(CLK_LOCAL_MEM_FENCE)` synchronizes all work-items in the work-group,
ensuring local memory writes are visible. It is cheap. There is **no portable
barrier across work-groups** inside a kernel launch — global synchronization
requires ending the kernel and starting a new one.

## Image objects (texture memory) {#image-objects}

OpenCL `image2d_t` objects go through a dedicated **texture cache** with
hardware-cached 2D spatial locality. Reading a grid cell and its 8 neighbors
through an `image2d_t` is much friendlier than a naked `__global` buffer of
identical content. For the grid and possibly the signal layer, image objects
are a natural fit *for read-only access*.

Caveat: writing to an image during the same kernel launch that reads it is
restricted. Read-only-during-parallel-phase is exactly the biosim4-gpu model,
so this is fine.

## Host-device transfer {#host-device-transfer}

Anything stored in host RAM must cross PCIe (or NVLink, or integrated memory)
to reach the GPU. This is the highest-latency, lowest-bandwidth link in the
system. **Every simStep that transfers data is a performance death sentence.**
The design keeps all per-simStep state resident on the GPU for the whole
generation. Only at the generation boundary does the host read back survivor
data and write the new population.
