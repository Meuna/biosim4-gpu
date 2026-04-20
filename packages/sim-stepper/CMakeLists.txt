find_package(Argtable3 CONFIG REQUIRED)

add_executable(biosim-stepper
  src/main.c
  src/cli.c
  src/toml.c
)
target_compile_features(biosim-stepper PRIVATE c_std_11)
target_include_directories(biosim-stepper PRIVATE include)
target_link_libraries(biosim-stepper PRIVATE core tomlc17 argtable3::argtable3)
install(TARGETS biosim-stepper RUNTIME)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
