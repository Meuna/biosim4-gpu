add_executable(biosim-stepper src/main.c)
target_compile_features(biosim-stepper PRIVATE c_std_11)
install(TARGETS biosim-stepper RUNTIME)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
