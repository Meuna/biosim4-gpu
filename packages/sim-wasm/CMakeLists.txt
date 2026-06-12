add_executable(biosim-wasm src/bindings.c)
target_compile_features(biosim-wasm PRIVATE c_std_11)
target_compile_definitions(biosim-wasm PRIVATE _GNU_SOURCE)
target_link_libraries(biosim-wasm PRIVATE core)

set_target_properties(biosim-wasm PROPERTIES
  OUTPUT_NAME "biosim"
  SUFFIX ".mjs"
)

target_link_options(biosim-wasm PRIVATE
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sENVIRONMENT=worker
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAP32,HEAPU8,HEAPU32
  -sALLOW_MEMORY_GROWTH=1
)
