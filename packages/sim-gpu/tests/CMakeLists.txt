add_executable(sim-gpu-k1-sensor-tests test_k1_sensors.c)
target_link_libraries(sim-gpu-k1-sensor-tests PRIVATE sim-gpu-lib core unity OpenCL::OpenCL)
target_compile_features(sim-gpu-k1-sensor-tests PRIVATE c_std_11)
add_test(NAME sim-gpu-k1-sensor-tests COMMAND sim-gpu-k1-sensor-tests)
