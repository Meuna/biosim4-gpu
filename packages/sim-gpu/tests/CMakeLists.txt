add_executable(sim-gpu-k1-feedforward-tests test_k1_feedforward.c)
target_link_libraries(sim-gpu-k1-feedforward-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k1-feedforward-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k1-feedforward-tests COMMAND sim-gpu-k1-feedforward-tests)

add_executable(sim-gpu-k2-movement-tests test_k2_movement_resolution.c)
target_link_libraries(sim-gpu-k2-movement-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k2-movement-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k2-movement-tests COMMAND sim-gpu-k2-movement-tests)
