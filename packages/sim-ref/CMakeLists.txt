add_executable(biosim-ref
  src/main.c
)
target_compile_features(biosim-ref PRIVATE c_std_11)
target_include_directories(biosim-ref PRIVATE include)
target_link_libraries(biosim-ref PRIVATE core cfgparse $<$<PLATFORM_ID:Linux>:m>)
target_compile_definitions(biosim-ref PRIVATE BIOSIM_PROGNAME="biosim-ref")
biosim_target_git_version(biosim-ref)
install(TARGETS biosim-ref RUNTIME)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
