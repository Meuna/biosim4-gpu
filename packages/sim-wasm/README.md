# sim-wasm

Emscripten-compiled WebAssembly module that exposes the biosim4-gpu simulation
engine to JavaScript. It is built as an ES6 module (`biosim.mjs` + `biosim.wasm`)
with `-sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=worker` so that it can be
loaded inside a Web Worker via a dynamic `import()`. The module exports a default
factory function; call it with `await createBiosim()` to obtain an instance, then
invoke C functions through `ccall`.

## Stepper API

All functions are called via `biosim.ccall(name, returnType, [], [])`.

| Function | Return | Description |
|---|---|---|
| `biosim_wasm_init` | `number` (status) | Create simulation with default parameters (same as `sim-ref`). |
| `biosim_wasm_free` | `null` | Release simulation resources. |
| `biosim_wasm_do_step` | `number` (status) | Run all agents for the current step, then finalize the step. |
| `biosim_wasm_do_step_agent` | `number` (status) | Run one alive agent; auto-finalizes the step when the last agent is processed. |
| `biosim_wasm_do_gen` | `number` (status) | Run all steps for the current generation, then advance the generation boundary. Equivalent to calling `biosim_wasm_do_step` until `biosim_wasm_is_gen_complete`, then `biosim_wasm_next_generation`. |
| `biosim_wasm_next_generation` | `number` (status) | Run the generational boundary (challenge eval, reproduction, respawn). Only valid when `is_gen_complete` is true. |
| `biosim_wasm_get_gen` | `number` | Current generation index. |
| `biosim_wasm_get_step` | `number` | Current step index within the generation (0 – steps_per_gen). |
| `biosim_wasm_get_last_agent` | `number` | Agent index processed by the last `do_step_agent` call. |
| `biosim_wasm_is_gen_complete` | `number` (bool) | Returns 1 when step ≥ steps_per_gen. |
| `biosim_wasm_census_gen` | `number` | Generation index from the last census. |
| `biosim_wasm_census_population` | `number` | Total agent slots from the last census. |
| `biosim_wasm_census_survivors` | `number` | Survivors from the last census. |
| `biosim_wasm_census_kills` | `number` | Kills from the last census. |

## Configuration API

Call parameter setters **before** `biosim_wasm_init`. They update a persistent
mutable copy of the defaults; calling `biosim_wasm_init` applies whatever values
are currently stored.

| Function | Args | Return | Description |
|---|---|---|---|
| `biosim_wasm_set_param_int` | `name: string, val: number` | `number` (status) | Override an integer parameter by name. |
| `biosim_wasm_set_param_float` | `name: string, val: number` | `number` (status) | Override a float parameter by name. |
| `biosim_wasm_set_param_bool` | `name: string, val: number` | `number` (status) | Override a boolean parameter by name (`val`: non-zero = true). |

Return codes: `0` = OK, `3` = name not found (`BIOSIM_ERR_NOTFOUND`), `2` = type
mismatch (`BIOSIM_ERR_TYPE`).

These are invoked via `ccall` with `argTypes: ['string', 'number']`:
```js
biosim.ccall('biosim_wasm_set_param_int', 'number', ['string', 'number'],
             ['population', 2000]);
biosim.ccall('biosim_wasm_init', 'number', [], []);
```

### Configurable parameters

| Name | Type | Default |
|---|---|---|
| `population` | int | 3000 |
| `grid-size-x` | int | 128 |
| `grid-size-y` | int | 128 |
| `steps-per-gen` | int | 300 |
| `max-generations` | int | 1000 |
| `max-genes` | int | 24 |
| `max-neurons` | int | 5 |
| `point-mutation-rate` | float | 0.001 |
| `sexual-reproduction` | bool | false |
| `choose-parents-by-fitness` | bool | false |
| `los-range` | int | 16 |
| `sensor-radius` | int | 2 |
| `enable-kill` | bool | false |
| `responsiveness-curve-k` | float | 2.0 |

## Barrier API

Call `biosim_wasm_clear_barriers` and `biosim_wasm_add_barrier` **before**
`biosim_wasm_init` to configure barrier shapes. The list is persistent across
`init` calls; call `clear_barriers` before rebuilding it.

| Function | Args | Return | Description |
|---|---|---|---|
| `biosim_wasm_clear_barriers` | — | `null` | Reset the barrier list to empty. |
| `biosim_wasm_add_barrier` | `kind: number, x: number, y: number, length: number, width: number, quadrant: number` | `number` (status) | Append one barrier. `x`/`y`/`length`/`width` are grid ratios in `[0, 1]`; pass `-1.0` for a random position or `0.0` for a random dimension. `quadrant` selects a corner's arm directions and is ignored by other kinds. Returns `BIOSIM_ERR_NOMEM` if allocation fails. |
| `biosim_wasm_get_n_barriers` | — | `number` | Number of barriers currently in the list. |

**Sentinel values for `add_barrier`:**
- `x` / `y`: pass `-32768` (`INT16_MIN`) for random placement.
- `length` / `width`: pass `0.0` for a random dimension.

**Kind integers** (match `biosim_barrier_kind_t`):

| Kind | Integer | Shape |
|---|---|---|
| `hbar` | 0 | Horizontal bar |
| `vbar` | 1 | Vertical bar |
| `square` | 2 | Filled square |
| `circle` | 3 | Filled circle |

Return code for `add_barrier`: `0` = OK, `4` = list full (`BIOSIM_ERR_INVALID`).

## Snapshot import / export API

These functions allow saving the current generation's survivor genome to memory
and restoring it later (deterministic round-trip via `gen_rng`).

**Export** (save to JS):

| Function | Args | Return | Description |
|---|---|---|---|
| `biosim_wasm_snapshot_export` | — | `number` (status) | Serialise the current survivor snap to an in-memory buffer. Returns `BIOSIM_ERR_INVALID` when no survivors are available (before the first generation boundary). |
| `biosim_wasm_snapshot_export_ptr` | — | `number` (uint32 pointer) | WASM heap pointer to the export buffer. Valid until the next `export` call. Read with `biosim.HEAPU8.slice(ptr, ptr + size)`. |
| `biosim_wasm_snapshot_export_size` | — | `number` (uint32 bytes) | Byte length of the export buffer. |

Typical export sequence:
```js
const rc = biosim.ccall('biosim_wasm_snapshot_export', 'number', [], []);
if (rc !== 0) throw new Error(`export failed: ${rc}`);
const ptr  = biosim.ccall('biosim_wasm_snapshot_export_ptr',  'number', [], []);
const size = biosim.ccall('biosim_wasm_snapshot_export_size', 'number', [], []);
const data = biosim.HEAPU8.slice(ptr, ptr + size); // independent copy
```

**Import** (restore from JS):

| Function | Args | Return | Description |
|---|---|---|---|
| `biosim_wasm_snapshot_import_alloc` | `size: number` | `number` (uint32 pointer) | Allocate a WASM-side import buffer; returns its heap pointer (0 on OOM). JS writes the snapshot bytes here. |
| `biosim_wasm_snapshot_import` | — | `number` (status) | Parse the import buffer and load the last generation's survivors into snap. Does **not** spawn — call `biosim_wasm_rewind` (or `_rewind_configured`) to breed from them. Frees the import buffer on return. |
| `biosim_wasm_snapshot_max_genes` | — | `number` (uint32) | Genome-length cap of the loaded snapshot's originating config. Compare against the live `max-genes` to decide whether breeding is compatible before rewinding. |
| `biosim_wasm_snapshot_max_neurons` | — | `number` (uint32) | Neuron cap of the loaded snapshot's originating config. Compare against the live `max-neurons` for the same compatibility decision. |

Typical import sequence (call after `biosim_wasm_init`):
```js
const ptr = biosim.ccall('biosim_wasm_snapshot_import_alloc', 'number',
                          ['number'], [data.byteLength]);
if (ptr === 0) throw new Error('alloc failed');
biosim.HEAPU8.set(data, ptr);
const rc = biosim.ccall('biosim_wasm_snapshot_import', 'number', [], []);
if (rc !== 0) throw new Error(`import failed: ${rc}`);
// Survivors are loaded but not yet bred. Verify compatibility, then spawn:
const needLen = biosim.ccall('biosim_wasm_snapshot_max_genes', 'number', [], []);
const needNeurons = biosim.ccall('biosim_wasm_snapshot_max_neurons', 'number', [], []);
// ... if needLen <= current max-genes && needNeurons <= current max-neurons:
biosim.ccall('biosim_wasm_rewind', 'number', [], []);
```

## Rendering / inspection queries

These functions return byte offsets into the Emscripten heap. Access the
corresponding WASM arrays via the appropriate typed view on the JavaScript side.
All indices are in `[0, population)`.

**Scalar queries** (called via `ccall(name, 'number', [], [])`):

| Function | Return | Description |
|---|---|---|
| `biosim_wasm_get_population` | `number` | Total agent slot count. |
| `biosim_wasm_get_kills` | `number` | Live kill count for the current generation (accumulates per step). |
| `biosim_wasm_get_size_x` | `number` | Grid width in cells. |
| `biosim_wasm_get_size_y` | `number` | Grid height in cells. |

**Pointer queries** (each returns `uint32_t` byte offset; divide by element
size for the typed-array index):

| Function | HEAP view | Element type | Description |
|---|---|---|---|
| `biosim_wasm_get_loc_x_ptr` | `HEAP32` | `int32` | Agent x grid coordinate. |
| `biosim_wasm_get_loc_y_ptr` | `HEAP32` | `int32` | Agent y grid coordinate. |
| `biosim_wasm_get_alive_ptr` | `HEAPU8` | `uint8` | 1 = alive, 0 = dead. |
| `biosim_wasm_get_birth_x_ptr` | `HEAP32` | `int32` | Spawn x coordinate. |
| `biosim_wasm_get_birth_y_ptr` | `HEAP32` | `int32` | Spawn y coordinate. |
| `biosim_wasm_get_last_move_dir_ptr` | `HEAPU8` | `uint8` | Last movement direction, 3-bit value: `0=E, CCW: E NE N NW W SW S SE`. |
| `biosim_wasm_get_osc_period_ptr` | `HEAPU16` | `uint16` | Oscillator period in steps. |
| `biosim_wasm_get_responsiveness_ptr` | `HEAPF32` | `float32` | Neural responsiveness scalar. |
| `biosim_wasm_get_los_range_ptr` | `HEAPU8` | `uint8` | Line-of-sight range in cells. |
| `biosim_wasm_get_challenge_bits_ptr` | `HEAPU32` | `uint32` | Per-step challenge-pass bitmask. |
| `biosim_wasm_get_genome_fingerprint_ptr` | `HEAPU32` | `uint64` (2×uint32 LE) | Genome hash set in `generation.c`. Read as `lo = HEAPU32[off + id*2]`, `hi = HEAPU32[off + id*2 + 1]`. |
| `biosim_wasm_get_grid_cells_ptr` | `HEAPU32` | `uint32` | Flat row-major grid: `cells[gy*W + gx]`. Encoding: `0` = empty, `0xFFFFFFFF` = barrier, else **1-based agent index** (`raw - 1` = agent id). |

**Brain (neural net) queries.** The connection and weight arrays are stored
column-major with a `slot * population + id` stride (`conn_slot` outer,
`agent_idx` inner). Read connection `slot` of agent `id` as
`view[slot * population + id]`. Packed connection genes are decoded with the
`gene.h` bit layout (`BIOSIM_GENE_SRC_TYPE` / `SRC_NUM` / `SINK_TYPE` /
`SINK_NUM`); weights are fixed-point, divide by `BIOSIM_GENE_WEIGHT_SCALE`
(8192). After slot compilation, IO source/sink numbers are real
`biosim_sensor_t` / `biosim_action_t` ordinals and neuron numbers are compact
`0..neuron_count-1`.

| Function | HEAP view | Element type | Description |
|---|---|---|---|
| `biosim_wasm_get_genome_conn_ptr` | `HEAPU16` | `uint16` | Packed conn genes `[slot * pop + id]`. |
| `biosim_wasm_get_genome_wgt_ptr` | `HEAPI16` | `int16` | Conn weights `[slot * pop + id]` (÷8192). |
| `biosim_wasm_get_conn_length_ptr` | `HEAPU16` | `uint16` | Active connection count `[id]`. |
| `biosim_wasm_get_neuron_count_ptr` | `HEAPU8` | `uint8` | Active neuron count `[id]`. |

## Default parameters

Matches `sim-ref` defaults: population 3000, grid 128×128, 300 steps/generation,
1000 max generations, genome length 24, 5 neurons, x-band challenge (x ∈ [0.5, 1.0]),
no barriers.
