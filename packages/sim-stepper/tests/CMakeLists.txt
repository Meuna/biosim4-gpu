add_executable(stepper-tests test_stepper.c)
target_link_libraries(stepper-tests PRIVATE unity)
target_compile_features(stepper-tests PRIVATE c_std_11)

add_test(NAME stepper-tests COMMAND stepper-tests)
