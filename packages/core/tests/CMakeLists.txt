add_executable(core-tests test_params.c)
target_link_libraries(core-tests PRIVATE core unity)
target_compile_features(core-tests PRIVATE c_std_11)
add_test(NAME core-tests COMMAND core-tests)

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
