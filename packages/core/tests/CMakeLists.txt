add_library(core-sim-test-utils STATIC sim_test_utils.c)
target_link_libraries(core-sim-test-utils PUBLIC core unity)
target_compile_features(core-sim-test-utils PUBLIC c_std_11)
target_include_directories(core-sim-test-utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

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

add_executable(core-io-eval-tests test_io_eval.c)
target_link_libraries(core-io-eval-tests PRIVATE core unity)
target_compile_features(core-io-eval-tests PRIVATE c_std_11)
add_test(NAME core-io-eval-tests COMMAND core-io-eval-tests)

add_executable(core-barriers-tests test_barriers.c)
target_link_libraries(core-barriers-tests PRIVATE core unity)
target_compile_features(core-barriers-tests PRIVATE c_std_11)
add_test(NAME core-barriers-tests COMMAND core-barriers-tests)

add_executable(core-challenge-tests test_challenges.c)
target_link_libraries(core-challenge-tests PRIVATE core-sim-test-utils)
target_compile_features(core-challenge-tests PRIVATE c_std_11)
add_test(NAME core-challenge-tests COMMAND core-challenge-tests)

add_executable(core-challenge-grid-tests test_challenges_grid.c)
target_link_libraries(core-challenge-grid-tests PRIVATE core-sim-test-utils)
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

add_executable(core-survivor-snap-tests test_survivor_snap.c)
target_link_libraries(core-survivor-snap-tests PRIVATE core unity)
target_compile_features(core-survivor-snap-tests PRIVATE c_std_11)
add_test(NAME core-survivor-snap-tests COMMAND core-survivor-snap-tests)

add_executable(core-generation-tests test_generation.c)
target_link_libraries(core-generation-tests PRIVATE core-sim-test-utils)
target_compile_features(core-generation-tests PRIVATE c_std_11)
add_test(NAME core-generation-tests COMMAND core-generation-tests)

add_executable(core-hud-tests test_hud.c)
target_link_libraries(core-hud-tests PRIVATE core unity)
target_compile_features(core-hud-tests PRIVATE c_std_11)
add_test(NAME core-hud-tests COMMAND core-hud-tests)

add_executable(core-terminal-tests test_terminal.c)
target_link_libraries(core-terminal-tests PRIVATE core unity)
target_compile_features(core-terminal-tests PRIVATE c_std_11)
add_test(NAME core-terminal-tests COMMAND core-terminal-tests)
