add_executable(biosim-stepper
  src/main.c
  src/step.c
  src/gen.c
)
target_compile_features(biosim-stepper PRIVATE c_std_11)
target_include_directories(biosim-stepper PRIVATE include)
target_link_libraries(biosim-stepper PRIVATE core params $<$<PLATFORM_ID:Linux>:m>)
target_compile_definitions(biosim-stepper PRIVATE BIOSIM_PROGNAME="biosim-stepper")
biosim_target_git_version(biosim-stepper)
install(TARGETS biosim-stepper RUNTIME)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
