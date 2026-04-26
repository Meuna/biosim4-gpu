add_library(core STATIC
  src/barriers.c
  src/challenges.c
  src/context.c
  src/core.c
  src/gen.c
  src/grid.c
  src/rng.c
  src/agents.c
  src/genome.c
  src/nnet.c
  src/io_catalogue.c
  src/step.c
)
target_compile_features(core PUBLIC c_std_11)
target_include_directories(core PUBLIC include)
target_link_libraries(core PRIVATE $<$<PLATFORM_ID:Linux>:m>)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
