add_executable(sim-gpu-k1-feedforward-tests test_k1_feedforward.c)
target_link_libraries(sim-gpu-k1-feedforward-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k1-feedforward-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k1-feedforward-tests COMMAND sim-gpu-k1-feedforward-tests)
