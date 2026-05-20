add_executable(biosim-wasm src/bindings.c)
target_compile_features(biosim-wasm PRIVATE c_std_11)
target_link_libraries(biosim-wasm PRIVATE core)

set_target_properties(biosim-wasm PROPERTIES
  OUTPUT_NAME "biosim"
  SUFFIX ".mjs"
)

target_link_options(biosim-wasm PRIVATE
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sENVIRONMENT=worker
  "-sEXPORTED_FUNCTIONS=_biosim_wasm_init,_biosim_wasm_free,_biosim_wasm_do_step,_biosim_wasm_do_step_agent,_biosim_wasm_next_generation,_biosim_wasm_get_gen,_biosim_wasm_get_step,_biosim_wasm_get_last_agent,_biosim_wasm_is_gen_complete,_biosim_wasm_census_gen,_biosim_wasm_census_population,_biosim_wasm_census_survivors,_biosim_wasm_census_kills"
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
  -sALLOW_MEMORY_GROWTH=1
)
