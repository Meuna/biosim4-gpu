add_executable(biosim-wasm-stepper src/main.c)
target_compile_features(biosim-wasm-stepper PRIVATE c_std_11)
target_link_libraries(biosim-wasm-stepper PRIVATE core)
target_compile_definitions(biosim-wasm-stepper PRIVATE BIOSIM_PROGNAME="biosim-wasm-stepper")

if(EMSCRIPTEN)
  set_target_properties(biosim-wasm-stepper PROPERTIES SUFFIX ".html")
  target_link_options(biosim-wasm-stepper PRIVATE
    -sEXIT_RUNTIME=1
    -sALLOW_MEMORY_GROWTH=1
  )
endif()
