add_executable(core-tests test_params.c)
target_link_libraries(core-tests PRIVATE core unity)
target_compile_features(core-tests PRIVATE c_std_11)

add_test(NAME core-tests COMMAND core-tests)
