# BioSim4 — Current Data Model Design Document

**Purpose:** Reference for GPU/SoA refactoring. Describes every field, its type and size, how it is
accessed during simulation, and identifies parallelization pitfalls.

## Table of Contents

1. [Primitive & Spatial Types](#1-primitive--spatial-types)
2. [Genome & Gene Representation](#2-genome--gene-representation)
3. [Neural Network](#3-neural-network)
4. [Individual Agent — `Indiv`](#4-individual-agent--indiv)
5. [Population Manager — `Peeps`](#5-population-manager--peeps)
6. [Spatial Grid — `Grid`](#6-spatial-grid--grid)
7. [Chemical Signals — `Signals`](#7-chemical-signals--signals)
8. [Sensor Catalogue](#8-sensor-catalogue)
9. [Action Catalogue](#9-action-catalogue)
10. [Simulation Loop & Phase Structure](#10-simulation-loop--phase-structure)
11. [Neural Network Execution — `feedForward`](#11-neural-network-execution--feedforward)
12. [Reproduction & Mutation](#12-reproduction--mutation)
13. [Current Memory Layout (AoS)](#13-current-memory-layout-aos)
14. [GPU/SoA Refactoring — Pitfalls & Recommendations](#14-gpusoa-refactoring--pitfalls--recommendations)

## 1. Primitive & Spatial Types

All types in `basicTypes.h` use `__attribute__((packed))`.

### `Compass` — 1 byte

```cpp
enum class Compass : uint8_t {
    SW=0, S, SE, W, CENTER, E, NW, N, NE   // 9 values
};
```

Used inside `Dir`. The `CENTER` value (4) signals "no movement".

### `Dir` — 1 byte

```cpp
struct Dir {
    Compass dir9;   // uint8_t
};
```

Key methods: `asNormalizedCoord()` → `Coord`, `rotate()`, `rotate90DegCW()`.
Stored in `Indiv::lastMoveDir`. Written once per successful move.

### `Coord` — 4 bytes

```cpp
struct Coord {
    int16_t x;   // 2 bytes
    int16_t y;   // 2 bytes
};
```

Used for all grid positions. **Read every simStep** for every sensor that needs
position or direction. Written only during the sequential drain phase.

### `Polar` — 5 bytes

```cpp
struct Polar {
    int mag;   // int32_t — 4 bytes
    Dir dir;   // 1 byte
};
```

Transient — computed inside sensor functions, never stored in `Indiv`.

## 2. Genome & Gene Representation

### `Gene` — 6 bytes (packed bitfield)

Defined in `genome-neurons.h`:

```cpp
struct Gene {
    uint16_t sourceType : 1;  // SENSOR(1) or NEURON(0)
    uint16_t sourceNum  : 7;  // index 0-127 — remapped modulo valid range
    uint16_t sinkType   : 1;  // ACTION(1) or NEURON(0)
    uint16_t sinkNum    : 7;  // index 0-127 — remapped modulo valid range
    int16_t  weight;          // signed 16-bit — see weightAsFloat() below
};
```

`weightAsFloat()` scales the raw integer to a small float and cubes it, giving
non-linear sensitivity: small weights stay small, large weights dominate.

`makeRandomWeight()` returns a random `int16_t`.

**Size:** 6 bytes. **Variable count per individual.**

### `Genome` — `std::vector<Gene>`

```cpp
typedef std::vector<Gene> Genome;
```

- **Minimum length:** 1 gene (enforced after mutation).
- **Maximum length:** `p.genomeMaxLength` (runtime parameter).
- **Heap-allocated**, pointer stored inside `Indiv`.
- Read every simStep during `feedForward`. Written only during reproduction.

## 3. Neural Network

Defined in `neuralNet.h`.

### `NeuralNet::Neuron` — 5 bytes

```cpp
struct Neuron {
    float output;   // 4 bytes — range -1.0..1.0 (tanh), or 0.5 if undriven
    bool  driven;   // 1 byte  — true if the neuron receives at least one input
};
```

`output` is updated every simStep and carried forward as input to the next step.

### `NeuralNet` — variable

```cpp
struct NeuralNet {
    std::vector<Gene>   connections;  // culled copy of genome synapses
    std::vector<Neuron> neurons;      // internal neurons only (no sensors/actions)
};
```

- Both vectors are **heap-allocated**.
- Maximum neuron count: `p.maxNumberNeurons`.
- `connections` is a **compacted** form of the genome: useless neurons (no output
  path, or pure self-loop) are removed by `createWiringFromGenome()`.
- Rebuilt once per generation at reproduction time; never modified mid-generation.

## 4. Individual Agent — `Indiv`

Defined in `indiv.h`. One instance exists per agent in `peeps.individuals`.

### Field Table

| Field | Type | Bytes | Mutable phase | Notes |
|---|---|---|---|---|
| `alive` | `bool` | 1 | Sequential (death queue drain) | `false` → agent skipped in parallel loop |
| `index` | `uint16_t` | 2 | Set at construction | 1-based; 0 is reserved |
| `loc` | `Coord` | 4 | Sequential (move queue drain) | Current grid cell |
| `birthLoc` | `Coord` | 4 | Set at construction | Used by some challenges |
| `age` | `unsigned` | 4 | Parallel (increment per step) | 0 → stepsPerGeneration |
| `oscPeriod` | `unsigned` | 4 | Parallel (SET_OSCILLATOR_PERIOD action) | 2–2048, exponentially scaled |
| `genome` | `std::vector<Gene>` | 24 + heap | Set at construction | Read in feedForward |
| `nnet` | `NeuralNet` | 48 + heap | Set at construction | Read/updated in feedForward |
| `responsiveness` | `float` | 4 | Parallel (SET_RESPONSIVENESS action) | 0.0–1.0; scales action magnitudes |
| `longProbeDist` | `unsigned` | 4 | Parallel (SET_LONGPROBE_DIST action) | 1–32; sensor reach |
| `lastMoveDir` | `Dir` | 1 | Sequential (move queue drain) | Direction of last successful move |
| `challengeBits` | `unsigned` | 4 | Sequential (challenge evaluation) | Bitmask of completed sub-goals |

**Total fixed-layout size:** ~100 bytes + two heap-allocated vectors (genome, nnet).

### Initialization Sequence

Called once per agent per generation inside `spawnNewGeneration`:

```cpp
indiv.index        = index_;
indiv.loc          = loc_;
indiv.birthLoc     = loc_;
indiv.age          = 0;
indiv.oscPeriod    = 34;           // constant default
indiv.alive        = true;
indiv.lastMoveDir  = Dir::random8();
indiv.responsiveness   = 0.5;
indiv.longProbeDist    = p.longProbeDistance;
indiv.challengeBits    = 0;
indiv.genome       = std::move(genome_);   // move semantics
indiv.nnet         = createWiringFromGenome(indiv.genome);
grid.set(loc_, index_);
```

## 5. Population Manager — `Peeps`

Defined in `peeps.h / peeps.cpp`.

### Internal Storage

```cpp
std::vector<Indiv>                         individuals;
// index 0 is a sentinel/unused slot; valid indices: 1..population
```

### Death Queue

```cpp
std::vector<uint16_t> deathQueue;
```

- Holds agent **indices** (not pointers).
- Appended in the parallel phase via `queueForDeath()`, protected by
  `#pragma omp critical`.
- Drained in `endOfSimStep()` (single-threaded):

```cpp
for (uint16_t idx : deathQueue) {
    grid.set(peeps[idx].loc, 0);   // erase from grid
    peeps[idx].alive = false;
}
deathQueue.clear();
```

### Move Queue

```cpp
std::vector<std::pair<uint16_t, Coord>> moveQueue;
```

- Each entry = `(agent_index, desired_destination)`.
- Appended in the parallel phase via `queueForMove()`, protected by
  `#pragma omp critical`.
- Drained in `endOfSimStep()` (single-threaded):

```cpp
for (auto& [idx, newLoc] : moveQueue) {
    if (peeps[idx].alive && grid.isEmptyAt(newLoc)) {
        grid.set(peeps[idx].loc, 0);
        peeps[idx].loc = newLoc;
        grid.set(newLoc, idx);
        peeps[idx].lastMoveDir = /* direction of move */;
    }
    // otherwise: move silently fails (contested or dead)
}
moveQueue.clear();
```

### Lookup

```cpp
Indiv & getIndiv(Coord loc);     // reads grid.at(loc) → index → individuals[index]
Indiv & operator[](uint16_t i);  // direct index access
```

`getIndiv(Coord)` is called inside the **GENETIC_SIM_FWD sensor** and internally
to resolve grid indices. It involves two memory indirections (grid cell → index
→ Indiv) and is a key source of non-local memory access.

## 6. Spatial Grid — `Grid`

Defined in `grid.h / grid.cpp`.

### Storage

```cpp
std::vector<Column> data;   // column-major: data[x][y]

struct Column {
    std::vector<uint16_t> data;
};
```

- **Layout:** column-major (`data[x][y]`), so iterating along a fixed `x` is
  cache-friendly.
- **Cell encoding:**

| Value | Meaning |
|---|---|
| `0x0000` | Empty cell |
| `0xffff` (65535) | Barrier |
| `0x0001`..`0xfffe` | Agent index (reference into `peeps.individuals`) |

- **Grid dimensions:** `p.sizeX × p.sizeY` (runtime parameters).

### Barrier Lists

```cpp
std::vector<Coord> barrierLocations;   // all barrier cells
std::vector<Coord> barrierCenters;     // cluster centroids
```

Populated once at generation start. Read-only during simulation steps.

### Key Methods

| Method | Phase | Access |
|---|---|---|
| `at(Coord)` | Parallel | Read — used by all proximity sensors |
| `set(Coord, val)` | Sequential | Write — used by queue drains |
| `isEmptyAt(Coord)` | Parallel | Read — movement validation |
| `isBarrierAt(Coord)` | Parallel | Read — BARRIER_FWD/LR sensors |
| `isOccupiedAt(Coord)` | Parallel | Read — POPULATION sensors |
| `visitNeighborhood(Coord, radius, fn)` | Parallel | Read — iterates cells within radius |
| `findEmptyLocation()` | Sequential | Read+scan — used at spawn time |
| `zeroFill()` | Sequential | Write — called at generation start |

## 7. Chemical Signals — `Signals`

Defined in `signals.h / signals.cpp`.

### Storage

```cpp
std::vector<Layer> data;

struct Layer  { std::vector<Column> data; };
struct Column { std::vector<uint8_t> data; };
// Access: data[layer][x][y]
```

- **Value range:** 0 (SIGNAL_MIN) to 255 (SIGNAL_MAX), `uint8_t` per cell.
- **Layer count:** `p.signalLayers` (typically 1).
- **Fully heap-allocated** with nested `std::vector`.

### Operations

| Operation | Phase | Notes |
|---|---|---|
| `getMagnitude(layer, loc)` | Parallel (read) | Sensor reads; no lock needed |
| `increment(layer, loc)` | Parallel (write) | Protected by `#pragma omp critical`; writes center (+2) and 1.5-radius neighbors (+1); clamped to 255 |
| `fade(layer)` | Sequential | Subtracts fadeAmount from every cell; clamps to 0 |
| `zeroFill()` | Sequential | Generation start reset |

`increment` is the only write that occurs during the parallel phase. Its
`omp critical` section is a **serialization bottleneck** on the CPU and a
**race condition hazard** on the GPU (requires atomic operations).

## 8. Sensor Catalogue

All sensors return `float` in range `0.0..1.0`. Implemented in `getSensor.cpp`.

The enum `Sensor` (21 entries + `NUM_SENSES` marker) is defined in
`sensors-actions.h`.

### Access Classification

#### Group A — Self-only (trivially parallelizable)

| Sensor | Fields read | Formula |
|---|---|---|
| `LOC_X` | `loc.x`, `p.sizeX` | `x / (sizeX - 1)` |
| `LOC_Y` | `loc.y`, `p.sizeY` | `y / (sizeY - 1)` |
| `BOUNDARY_DIST_X` | `loc.x`, `p.sizeX` | `min(x, sizeX-x-1) / (sizeX/2)` |
| `BOUNDARY_DIST_Y` | `loc.y`, `p.sizeY` | `min(y, sizeY-y-1) / (sizeY/2)` |
| `BOUNDARY_DIST` | `loc`, both sizes | `min(distX, distY) / maxPossible` |
| `LAST_MOVE_DIR_X` | `lastMoveDir` | Normalized X component: 0.0 / 0.5 / 1.0 |
| `LAST_MOVE_DIR_Y` | `lastMoveDir` | Normalized Y component: 0.0 / 0.5 / 1.0 |
| `OSC1` | `oscPeriod`, `simStep` | `(1 - cos(phase × 2π)) / 2` |
| `AGE` | `age`, `p.stepsPerGeneration` | `age / stepsPerGeneration` |
| `RANDOM` | — | `randomUint() / UINT_MAX` |

#### Group B — Grid-dependent (reads shared, immutable-during-step grid)

These read `grid.at()` or iterate a neighborhood. The grid is **read-only**
during the parallel phase, so no locking is needed, but they cause irregular
(non-coalesced) memory access patterns.

| Sensor | Grid access pattern |
|---|---|
| `LONGPROBE_POP_FWD` | Ray-cast forward up to `longProbeDist` cells; counts occupied cells |
| `LONGPROBE_BAR_FWD` | Ray-cast forward up to `longProbeDist` cells; counts barriers |
| `POPULATION` | `visitNeighborhood(loc, populationSensorRadius)` — circular area |
| `POPULATION_FWD` | Weighted sum of cells along forward direction |
| `POPULATION_LR` | Weighted sum of cells along left/right direction |
| `BARRIER_FWD` | Short probe (up to `shortProbeBarrierDistance`) forward |
| `BARRIER_LR` | Short probe left/right |

#### Group C — Signal-dependent (reads shared, immutable-during-step signal layer)

Signal values are read-only during feedForward (writes via EMIT_SIGNAL0 are
queued implicitly through the critical section, but `getMagnitude` reads do not
conflict with concurrent `increment` writes in the current CPU model — this is
**not safe** in a GPU model without explicit ordering).

| Sensor | Signal access |
|---|---|
| `SIGNAL0` | `signals[0].getMagnitude(loc)` — single cell |
| `SIGNAL0_FWD` | Weighted sum of cells along forward direction |
| `SIGNAL0_LR` | Weighted sum of cells along left/right direction |

#### Group D — Cross-agent (reads another `Indiv` — the most challenging for GPU)

| Sensor | Description |
|---|---|
| `GENETIC_SIM_FWD` | Looks up the agent occupying the cell directly ahead: `peeps.getIndiv(forwardCell)`. Reads that agent's **genome** and computes a similarity score (Hamming-like) against the current agent's genome. Returns 0 if the forward cell is empty or a barrier. |

`GENETIC_SIM_FWD` is the only sensor that reads another individual's data. It
involves two pointer dereferences (grid → index → Indiv) and a genome traversal
of **variable length**, making it the hardest sensor to parallelize on a GPU.

## 9. Action Catalogue

Actions read from the `float[NUM_ACTIONS]` array returned by `feedForward` and
are executed in `executeActions.cpp`. Implemented after the parallel
`feedForward` pass (still within the parallel phase per-agent, but only queuing,
no direct state mutation of shared data).

### Action Table

| Action | Written field / effect | Queue used |
|---|---|---|
| `SET_RESPONSIVENESS` | `indiv.responsiveness` ← tanh(val)×0.5+0.5 | None (direct write) |
| `SET_OSCILLATOR_PERIOD` | `indiv.oscPeriod` ← exponential scale to 2–2048 | None (direct write) |
| `SET_LONGPROBE_DIST` | `indiv.longProbeDist` ← scaled to 1–32 | None (direct write) |
| `MOVE_X` | Accumulates X offset | `moveQueue` |
| `MOVE_Y` | Accumulates Y offset | `moveQueue` |
| `MOVE_FORWARD` | Direction = `lastMoveDir` | `moveQueue` |
| `MOVE_REVERSE` | Direction = opposite `lastMoveDir` | `moveQueue` |
| `MOVE_LEFT` | Direction = 90° left of `lastMoveDir` | `moveQueue` |
| `MOVE_RIGHT` | Direction = 90° right of `lastMoveDir` | `moveQueue` |
| `MOVE_RL` | Weighted combination of left + right | `moveQueue` |
| `MOVE_RANDOM` | Random direction | `moveQueue` |
| `MOVE_EAST/WEST/NORTH/SOUTH` | Fixed cardinal directions | `moveQueue` |
| `EMIT_SIGNAL0` | `signals[0].increment(loc)` | `omp critical` write |
| `KILL_FORWARD` | `peeps.queueForDeath(neighbor)` if value ≥ threshold | `deathQueue` |

**Important:** `SET_RESPONSIVENESS`, `SET_OSCILLATOR_PERIOD`, and
`SET_LONGPROBE_DIST` write directly to `Indiv` fields **within the parallel
phase**. They only write to the **current agent's own fields**, so there is no
data race on the CPU. On a GPU, this remains safe as long as each thread owns
exactly one agent.

## 10. Simulation Loop & Phase Structure

```
for generation in 0..maxGenerations:
    for simStep in 0..stepsPerGeneration:

        /* ── PARALLEL PHASE ───────────────────────────────────── */
        #pragma omp for
        for each alive Indiv in peeps.individuals[1..population]:
            indiv.age++
            actionVec = indiv.feedForward(simStep)  // reads: genome, nnet, grid, signals
            executeActions(indiv, actionVec)         // writes: self fields, queues, signals

        /* ── SEQUENTIAL PHASE ─────────────────────────────────── */
        #pragma omp single
        peeps.drainMoveQueue()    // writes: grid, Indiv.loc, Indiv.lastMoveDir
        peeps.drainDeathQueue()   // writes: grid, Indiv.alive
        evaluateChallenges()      // may write: Indiv.challengeBits, deathQueue
        signals.fade(0)           // writes: all signal cells

    /* ── GENERATION END (SEQUENTIAL) ──────────────────────────── */
    endOfGeneration()             // video / logging only
    spawnNewGeneration()          // rebuild all Indiv from survivor genomes
```

### Shared State Invariant

During the parallel phase:
- **Grid** is read-only (no agent can successfully move until queue drain).
- **Signals** values are readable without locks; `increment` uses a critical
  section.
- **Other agents' `Indiv` fields** are read-only *except* for `KILL_FORWARD`
  which only queues — it never directly modifies the target.
- Each agent writes only to **its own** `Indiv` fields and to the shared queues
  (protected).

This invariant is what makes the CPU OpenMP model safe. It must be preserved
under any GPU refactoring.

## 11. Neural Network Execution — `feedForward`

Implemented in `feedForward.cpp`.

### Data Flow

```
Sensors (21 floats, 0.0..1.0)
    ↓  (via nnet.connections where sourceType == SENSOR)
Neuron accumulators  ←  previous neuron outputs (from last simStep)
    ↓  tanh()
Neuron outputs (updated in place, -1.0..1.0)
    ↓  (via nnet.connections where sinkType == ACTION)
Action sums (raw accumulated floats, arbitrary range)
```

### Pseudocode

```cpp
// Phase 1: evaluate all 21 sensors
float sensorVal[NUM_SENSES];
for s in 0..NUM_SENSES:
    sensorVal[s] = getSensor(s, simStep)

// Phase 2: accumulate weighted inputs
float neuronAcc[nnet.neurons.size()] = {0};
float actionSum[NUM_ACTIONS]         = {0};

for conn in nnet.connections:
    inputVal = (conn.sourceType == SENSOR)
               ? sensorVal[conn.sourceNum]
               : nnet.neurons[conn.sourceNum].output
    weighted = inputVal * conn.weightAsFloat()
    if conn.sinkType == ACTION:
        actionSum[conn.sinkNum] += weighted
    else:
        neuronAcc[conn.sinkNum] += weighted

// Phase 3: apply activation function
for i in 0..neurons.size():
    if nnet.neurons[i].driven:
        nnet.neurons[i].output = tanh(neuronAcc[i])
    else:
        nnet.neurons[i].output = 0.5   // constant for undriven neurons

return actionSum
```

### Key Properties

- Connection count and neuron count **vary per agent** (depends on genome after
  culling).
- `nnet.neurons[i].output` carries state across simSteps — it is both read and
  written within the same `feedForward` call (previous output read in Phase 2,
  new output written in Phase 3).
- The connection list is ordered so that all neuron-sink connections appear
  before action-sink connections, enabling a single linear pass.

## 12. Reproduction & Mutation

Implemented in `genome.cpp` and `spawnNewGeneration.cpp`.

### Survivor Collection

```cpp
std::vector<Genome> parentGenomes;
for Indiv in peeps.individuals:
    if indiv.alive:
        parentGenomes.push_back(indiv.genome)
```

If `parentGenomes` is empty, generation 0 is re-initialized with random genomes.

### Child Genome Generation

**Asexual:**
```cpp
child = clone(*selectedParent)
```

**Sexual:**
```cpp
longer  = max(parent1, parent2) by genome length
shorter = min(parent1, parent2) by genome length
child   = copy of longer
// Overlay a random contiguous slice of shorter onto a random position in child
sliceStart  = randomUint(0, shorter.size()-1)
sliceLen    = randomUint(1, shorter.size()-sliceStart)
overlapStart= randomUint(0, child.size()-1)
for i in 0..sliceLen:
    child[overlapStart+i] = shorter[sliceStart+i]  // if in bounds
// Trim toward average parent length (50% probability)
targetLen = (parent1.size() + parent2.size()) / 2
if child.size() > targetLen && coin:
    child.erase(targetLen..)
```

### Mutation Operators

**Insertion/Deletion** (applied once, gated by `p.geneInsertionDeletionRate`):

```
if random < deletionRatio:  remove one random gene
else:                       insert one random gene (if below genomeMaxLength)
```

**Point Mutations** (applied per gene, gated by `p.pointMutationRate`):

```
pick one of 5 fields uniformly:
  case 0: gene.sourceType ^= 1
  case 1: gene.sinkType   ^= 1
  case 2: gene.sourceNum  ^= (1 << random(0..6))
  case 3: gene.sinkNum    ^= (1 << random(0..6))
  case 4: gene.weight     ^= (1 << random(1..15))
```

The weight flip targets higher-order bits more often (bits 1–15), which produces
larger perturbations — intentional design for cubed weight scaling.

### Post-Mutation Validity

```cpp
if child.empty():       child.push_back(Gene::makeRandom())
if child.size() > max:  child.erase(max..)
```

## 13. Current Memory Layout (AoS)

`peeps.individuals` is a **contiguous `std::vector<Indiv>`**, but each `Indiv`
contains two heap-allocated `std::vector` members (`genome`, `nnet.connections`,
`nnet.neurons`). This means the actual simulation data is scattered across the
heap.

### Per-Agent In-Memory Footprint

```
Indiv (on vector heap, contiguous):
  bool       alive             1 byte
  uint16_t   index             2 bytes
  Coord      loc               4 bytes   ← read every step
  Coord      birthLoc          4 bytes
  unsigned   age               4 bytes   ← read every step
  unsigned   oscPeriod         4 bytes   ← read every step (OSC1 sensor)
  Genome     genome            24 bytes (std::vector header) + heap ptr
  NeuralNet  nnet              48 bytes (two std::vector headers) + heap ptr
  float      responsiveness    4 bytes
  unsigned   longProbeDist     4 bytes
  Dir        lastMoveDir       1 byte    ← read every step
  unsigned   challengeBits     4 bytes
                               ≈ 104 bytes fixed + 2 heap allocations

Genome heap block (variable):
  Gene[genomeLength]           6 × genomeLength bytes

NeuralNet heap blocks (variable):
  Gene[connectionCount]        6 × connectionCount bytes
  Neuron[neuronCount]          5 × neuronCount bytes
```

### Cache Behavior

A simulation step over `N` agents with genome length `G` and neuron count `K`
touches approximately:

- `N × 104` bytes of contiguous `Indiv` structs (hot, likely cached)
- `N` independent heap blocks for genomes (cold, pointer-chased)
- `N` independent heap blocks for neural connections (cold, pointer-chased)
- `N` independent heap blocks for neuron states (cold, pointer-chased)
- Up to `sizeX × sizeY × 2` bytes of grid (shared, potentially warm)
- Up to `sizeX × sizeY × layers` bytes of signals (shared, potentially warm)

The pointer-chasing into genome and nnet vectors is the primary cache inefficiency
in the current design.

## 14. GPU/SoA Refactoring — Pitfalls & Recommendations

This section maps every data structure and access pattern identified above to
specific challenges in a GPU/SoA context. No refactoring is proposed here —
only the pitfalls are documented.

### 14.1 Variable-Length Genomes and Neural Networks

**Pitfall:** Each agent has a genome of a different length (1 to
`genomeMaxLength` genes) and a neural network with a different number of
connections and neurons. GPU kernels require uniform memory access patterns.
A naïve SoA with fixed-size arrays wastes memory and still causes warp
divergence on agents with shorter genomes.

**Options to consider (not decided here):**
- Fixed-capacity arrays padded to `genomeMaxLength` — wastes memory proportional
  to unused capacity.
- A flat genome array with per-agent offset and length arrays
  (`genomeOffsets[i]`, `genomeLengths[i]`) — compact but causes irregular
  gather access.
- Sorting agents by genome length before kernel launch to improve warp
  coherence.

The same three options apply to `nnet.connections` and `nnet.neurons`.

### 14.2 The `GENETIC_SIM_FWD` Sensor (Cross-Agent Dependency)

**Pitfall:** This sensor looks up the agent in the cell directly ahead of the
current agent, then reads that agent's **genome**. On a GPU:

1. **Two-level indirection:** grid cell → agent index → genome pointer. Both
   dereferences are likely L2 misses.
2. **Variable-length read:** The neighbor's genome may be longer or shorter than
   the current agent's genome. The loop over genome pairs cannot be unrolled at
   compile time.
3. **Write–read dependency:** In principle, two agents could be forward neighbors
   of each other in the same step. With the current model both only read, so
   there is no write conflict — but the dependency must be verified to remain
   one-directional after any SoA transformation.
4. **Warp divergence:** Some threads will have an empty cell ahead (return 0.0
   immediately), others will execute a full genome comparison loop. This creates
   severe warp divergence in a GPU warp of 32 threads.

**Recommendation to assess:** Whether to (a) execute `GENETIC_SIM_FWD` in a
separate, dedicated kernel pass with special memory layout, or (b) accept the
divergence and rely on warp-level masking.

### 14.3 Population Sensors (Neighborhood Queries)

**Pitfall:** `POPULATION`, `POPULATION_FWD`, `POPULATION_LR`, and the
`LONGPROBE_*` sensors iterate over a neighborhood of cells in the grid (circular
radius or forward ray). Each cell lookup is a random read from the grid array.

- The grid access pattern is **data-dependent** (which cells are populated
  depends on the current simulation state) and therefore unpredictable.
- `LONGPROBE_POP_FWD` may read up to `longProbeDist` cells (up to 32) per
  sensor per agent, with early exit on the first occupied cell. Early exit on
  GPU means warp divergence.
- `visitNeighborhood` with a circular radius causes different agents to read
  different non-contiguous grid regions — no coalescing.

### 14.4 Signal Emission (Write During Parallel Phase)

**Pitfall:** `EMIT_SIGNAL0` calls `signals[0].increment(loc)` with a
`#pragma omp critical` on the CPU. On a GPU, this must become an **atomic
operation**. The current `increment` implementation writes to the center cell
(+2) and up to ~7 neighbor cells (+1) within a 1.5-radius neighborhood, all
clamped to 255.

A single `EMIT_SIGNAL0` action triggers **up to 8 atomic writes** at different
addresses. If many agents emit signals simultaneously (a likely emergent
behavior), this can cause high contention on GPU atomic units.

Additionally, `uint8_t` atomics are not available in all GPU compute levels
(`atomicAdd` on bytes may require 32-bit word masking tricks).

### 14.5 Move and Death Queues (Conflict Resolution)

**Pitfall:** Two agents may request to move to the same destination cell. The
current CPU implementation resolves this naturally by processing the
`moveQueue` sequentially — the first agent to be processed takes the cell; the
second finds it occupied and silently fails.

On a GPU, if queue drain is parallelized:
- Two threads could concurrently call `grid.set(newLoc, idx)`, producing a
  **data race** that leaves the grid in an inconsistent state.
- The resolution order (which agent "wins") must be deterministic if simulation
  reproducibility is desired. The CPU's sequential drain gives an implicit
  ordering; any GPU parallel drain must define an equivalent.

The same concern applies to the `deathQueue` for `KILL_FORWARD`: two agents
could both kill the same neighbor, queuing the same index twice.

### 14.6 Direct Writes to Self-Fields During Parallel Phase

**Pitfall:** Actions `SET_RESPONSIVENESS`, `SET_OSCILLATOR_PERIOD`, and
`SET_LONGPROBE_DIST` write directly to `indiv.responsiveness`,
`indiv.oscPeriod`, and `indiv.longProbeDist` within the parallel phase (not via
a queue).

On the CPU this is safe because each thread owns one agent. On the GPU it remains
safe **only if** the SoA layout assigns each GPU thread a unique agent index
with no overlap. This must be guaranteed by the kernel launch configuration.

However, note that `oscPeriod` is read by the `OSC1` sensor in the *same*
`feedForward` call. If `SET_OSCILLATOR_PERIOD` fires before `OSC1` is evaluated
in the same step, the period change takes effect immediately. This ordering
depends on which action index is processed first in `executeActions`, and may
produce different results depending on connection order in the compiled neural
network. This subtle dependency must be preserved in any SoA refactoring.

### 14.7 Neuron State Carries Across Steps

**Pitfall:** `nnet.neurons[i].output` is both read (Phase 2 of `feedForward`)
and written (Phase 3 of `feedForward`) within the same step. The value written
in step `t` is the input for step `t+1`.

In a SoA layout with two arrays (`neuronOutput_A` and `neuronOutput_B`) and
ping-pong buffering, this is straightforward. However, the current code does
it in-place (writes Phase 3 output into the same location read by Phase 2) using
a separate accumulator — the accumulator is local, not the neuron array itself,
so there is no intra-step read-after-write hazard. This must be preserved
carefully: the SoA refactoring must not inadvertently allow a neuron output
written in Phase 3 of one connection's processing to be read by another
connection's Phase 2 in the same step.

The current loop processes all connections in a single pass into local
accumulators, then applies tanh in a second loop. This separation is the
invariant that makes it safe and must be maintained.

### 14.8 `age` Increment in the Parallel Phase

**Pitfall:** `indiv.age++` is performed inside `simStepOneIndiv`, which runs in
the parallel phase. This is a **read-modify-write** on `Indiv.age`. On the CPU
with OpenMP, each thread owns its own agent, so this is fine. On the GPU, each
thread must own exactly one agent with no aliasing.

### 14.9 Signal Fade — Full-Grid Sequential Write

**Pitfall:** `signals.fade(layer)` iterates every cell of the signal grid and
decrements it. This is a pure embarrassingly parallel operation (no
inter-cell dependencies) but it runs **single-threaded** in the current code as
part of the sequential end-of-step phase. On a GPU this is an ideal kernel, but
it must be launched after all `EMIT_SIGNAL0` atomics from the parallel phase have
completed — requiring an explicit **grid-level synchronization barrier** between
the two phases.

### 14.10 Challenges — Heterogeneous Evaluation Logic

**Pitfall:** The 19 challenge types are evaluated in `endOfSimStep` with
per-challenge branching logic. Some challenges (e.g., `ALTRUISM`,
`ALTRUISM_SACRIFICE`) inspect relationships between agents, requiring
cross-agent reads similar to `GENETIC_SIM_FWD`. Others (e.g., `MIGRATE_DISTANCE`)
compute per-agent scalars from `birthLoc` and `loc`.

Parallelizing challenge evaluation is feasible for the scalar challenges but
requires care for relational challenges. Since challenges run in the sequential
phase on the CPU, they are the lower priority for GPU acceleration but should
not be forgotten.

### 14.11 Random Number Generation

**Pitfall:** Several sensors (`RANDOM`) and actions (`MOVE_RANDOM`) consume
random numbers. The current code uses a global (or thread-local) RNG. On a GPU,
each thread must have its own RNG state. The `RANDOM` sensor returns a value in
`0.0..1.0`; its statistical properties affect evolutionary dynamics, so the RNG
quality must be maintained.

GPU-friendly options include per-thread xorshift64 or cuRAND device states, but
these must be seeded and stored in SoA layout alongside other per-agent fields.

### Summary Table of Pitfalls

| Pitfall | Severity | Root Cause |
|---|---|---|
| Variable-length genome & nnet | High | `std::vector` per agent; no fixed stride |
| `GENETIC_SIM_FWD` cross-agent genome read | High | Pointer chase + variable loop + warp divergence |
| Signal `increment` atomic contention | Medium | Up to 8 atomic writes per agent per step |
| Move queue conflict resolution | Medium | Two agents targeting same cell must serialize |
| `uint8_t` atomic for signal cells | Medium | Not universally supported without masking |
| Neuron in-place update invariant | Medium | Must keep accumulator-then-apply order |
| Death queue duplicate entries | Low | Same index may be queued twice |
| `age` read-modify-write | Low | Safe if one thread per agent |
| Oscillator period / OSC1 ordering | Low | Action fires before sensor reads in same step |
| Signal fade synchronization barrier | Low | Fade must follow all emit atomics |
| Challenge cross-agent evaluation | Low | Same as GENETIC_SIM_FWD but less frequent |
| Per-thread RNG state | Low | Global RNG must be split per GPU thread |
