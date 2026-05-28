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
| `max-genome-len` | int | 24 |
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
| `biosim_wasm_add_barrier` | `kind: number, x: number, y: number, length: number, width: number` | `number` (status) | Append one barrier. Up to 8 barriers supported. |
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

## Default parameters

Matches `sim-ref` defaults: population 3000, grid 128×128, 300 steps/generation,
1000 max generations, genome length 24, 5 neurons, x-band challenge (x ∈ [0.5, 1.0]),
no barriers.
