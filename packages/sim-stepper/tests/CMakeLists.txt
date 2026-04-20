find_package(Argtable3 CONFIG REQUIRED)

add_executable(stepper-tests
  test_cli.c
  ../src/toml.c
  ../src/cli.c
)
target_link_libraries(stepper-tests PRIVATE core tomlc17 argtable3::argtable3 unity)
target_compile_features(stepper-tests PRIVATE c_std_11)
target_include_directories(stepper-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_compile_definitions(stepper-tests PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
  BIOSIM_PROGNAME="biosim-stepper"
)

add_test(NAME stepper-tests COMMAND stepper-tests)
