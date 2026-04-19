add_executable(biosim-stepper src/main.c)
target_compile_features(biosim-stepper PRIVATE c_std_11)
target_link_libraries(biosim-stepper PRIVATE core)
install(TARGETS biosim-stepper RUNTIME)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
