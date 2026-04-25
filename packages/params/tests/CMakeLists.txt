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

add_executable(params-barriers-tests test_barriers.c)
target_link_libraries(params-barriers-tests PRIVATE params unity)
target_compile_features(params-barriers-tests PRIVATE c_std_11)
target_compile_definitions(params-barriers-tests PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME params-barriers-tests COMMAND params-barriers-tests)

add_executable(params-challenge-tests test_challenges.c)
target_link_libraries(params-challenge-tests PRIVATE params unity)
target_compile_features(params-challenge-tests PRIVATE c_std_11)
target_compile_definitions(params-challenge-tests PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME params-challenge-tests COMMAND params-challenge-tests)
