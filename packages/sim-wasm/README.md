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

## Default parameters

Matches `sim-ref` defaults: population 3000, grid 128×128, 300 steps/generation,
1000 max generations, genome length 24, 5 neurons, x-band challenge (x ∈ [0.5, 1.0]).
