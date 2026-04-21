add_executable(core-tests test_params.c)
target_link_libraries(core-tests PRIVATE core unity)
target_compile_features(core-tests PRIVATE c_std_11)
add_test(NAME core-tests COMMAND core-tests)

add_executable(core-grid-tests test_grid.c)
target_link_libraries(core-grid-tests PRIVATE core unity)
target_compile_features(core-grid-tests PRIVATE c_std_11)
add_test(NAME core-grid-tests COMMAND core-grid-tests)
