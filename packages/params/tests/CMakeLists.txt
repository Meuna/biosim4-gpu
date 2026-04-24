add_executable(params-tests test_params.c)
target_link_libraries(params-tests PRIVATE params unity)
target_compile_features(params-tests PRIVATE c_std_11)
add_test(NAME params-tests COMMAND params-tests)

add_executable(params-cli-tests test_cli.c)
target_link_libraries(params-cli-tests PRIVATE params unity)
target_compile_features(params-cli-tests PRIVATE c_std_11)
target_compile_definitions(params-cli-tests PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME params-cli-tests COMMAND params-cli-tests)
