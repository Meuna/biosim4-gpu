add_library(core-sim-test-utils STATIC sim_test_utils.c)
target_link_libraries(core-sim-test-utils PUBLIC core unity)
target_compile_features(core-sim-test-utils PUBLIC c_std_11)

add_executable(core-grid-tests test_grid.c)
target_link_libraries(core-grid-tests PRIVATE core unity)
target_compile_features(core-grid-tests PRIVATE c_std_11)
add_test(NAME core-grid-tests COMMAND core-grid-tests)

add_executable(core-rng-tests test_rng.c)
target_link_libraries(core-rng-tests PRIVATE core unity)
target_compile_features(core-rng-tests PRIVATE c_std_11)
add_test(NAME core-rng-tests COMMAND core-rng-tests)

add_executable(core-agent-tests test_agents.c)
target_link_libraries(core-agent-tests PRIVATE core unity)
target_compile_features(core-agent-tests PRIVATE c_std_11)
add_test(NAME core-agent-tests COMMAND core-agent-tests)

add_executable(core-genome-tests test_genome.c)
target_link_libraries(core-genome-tests PRIVATE core unity)
target_compile_features(core-genome-tests PRIVATE c_std_11)
add_test(NAME core-genome-tests COMMAND core-genome-tests)

add_executable(core-nnet-tests test_nnet.c)
target_link_libraries(core-nnet-tests PRIVATE core unity)
target_compile_features(core-nnet-tests PRIVATE c_std_11)
add_test(NAME core-nnet-tests COMMAND core-nnet-tests)

add_executable(core-io-catalogue-tests test_io_catalogue.c)
target_link_libraries(core-io-catalogue-tests PRIVATE core unity)
target_compile_features(core-io-catalogue-tests PRIVATE c_std_11)
add_test(NAME core-io-catalogue-tests COMMAND core-io-catalogue-tests)

add_executable(core-barriers-tests test_barriers.c)
target_link_libraries(core-barriers-tests PRIVATE core unity)
target_compile_features(core-barriers-tests PRIVATE c_std_11)
add_test(NAME core-barriers-tests COMMAND core-barriers-tests)

add_executable(core-challenge-tests test_challenges.c)
target_link_libraries(core-challenge-tests PRIVATE core unity)
target_compile_features(core-challenge-tests PRIVATE c_std_11)
add_test(NAME core-challenge-tests COMMAND core-challenge-tests)

add_executable(core-challenge-grid-tests test_challenges_grid.c)
target_link_libraries(core-challenge-grid-tests PRIVATE core unity)
target_compile_features(core-challenge-grid-tests PRIVATE c_std_11)
add_test(NAME core-challenge-grid-tests COMMAND core-challenge-grid-tests)

add_executable(core-snapshot-tests test_snapshot.c)
target_link_libraries(core-snapshot-tests PRIVATE core-sim-test-utils)
target_compile_features(core-snapshot-tests PRIVATE c_std_11)
target_compile_definitions(core-snapshot-tests PRIVATE
    BIOSIM_TEST_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME core-snapshot-tests COMMAND core-snapshot-tests)

add_executable(core-params-tests test_params.c)
target_link_libraries(core-params-tests PRIVATE core unity)
target_compile_features(core-params-tests PRIVATE c_std_11)
add_test(NAME core-params-tests COMMAND core-params-tests)
