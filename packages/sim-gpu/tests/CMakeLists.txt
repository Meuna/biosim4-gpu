add_executable(sim-gpu-k1-feedforward-tests test_k1_feedforward.c)
target_link_libraries(sim-gpu-k1-feedforward-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k1-feedforward-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k1-feedforward-tests COMMAND sim-gpu-k1-feedforward-tests)

add_executable(sim-gpu-k2-kill-marked-tests test_k2_kill_marked.c)
target_link_libraries(sim-gpu-k2-kill-marked-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k2-kill-marked-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k2-kill-marked-tests COMMAND sim-gpu-k2-kill-marked-tests)

add_executable(sim-gpu-k3-movement-tests test_k3_movement_resolution.c)
target_link_libraries(sim-gpu-k3-movement-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k3-movement-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k3-movement-tests COMMAND sim-gpu-k3-movement-tests)

add_executable(sim-gpu-k4-signal-fade-tests test_k4_signal_fade.c)
target_link_libraries(sim-gpu-k4-signal-fade-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k4-signal-fade-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k4-signal-fade-tests COMMAND sim-gpu-k4-signal-fade-tests)
