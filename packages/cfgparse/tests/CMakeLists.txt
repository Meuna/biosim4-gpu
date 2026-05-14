add_executable(cfgparse-cli-tests test_cli.c)
target_link_libraries(cfgparse-cli-tests PRIVATE cfgparse unity)
target_compile_features(cfgparse-cli-tests PRIVATE c_std_11)
target_compile_definitions(cfgparse-cli-tests PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME cfgparse-cli-tests COMMAND cfgparse-cli-tests)

add_executable(cfgparse-barriers-tests test_barriers.c)
target_link_libraries(cfgparse-barriers-tests PRIVATE cfgparse unity)
target_compile_features(cfgparse-barriers-tests PRIVATE c_std_11)
target_compile_definitions(cfgparse-barriers-tests PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME cfgparse-barriers-tests COMMAND cfgparse-barriers-tests)

add_executable(cfgparse-challenge-tests test_challenges.c)
target_link_libraries(cfgparse-challenge-tests PRIVATE cfgparse unity)
target_compile_features(cfgparse-challenge-tests PRIVATE c_std_11)
target_compile_definitions(cfgparse-challenge-tests PRIVATE
  TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME cfgparse-challenge-tests COMMAND cfgparse-challenge-tests)
