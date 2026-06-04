add_library(core STATIC
  src/barriers.c
  src/census.c
  src/challenges.c
  src/generation.c
  src/log.c
  src/params.c
  src/sim.c
  src/grid.c
  src/rng.c
  src/agents.c
  src/genome.c
  src/nnet.c
  src/io_eval.c
  src/snapshot.c
  src/status.c
  src/survivor_snap.c
)
target_compile_features(core PUBLIC c_std_11)
target_include_directories(core PUBLIC include)
target_link_libraries(core PRIVATE $<$<PLATFORM_ID:Linux>:m>)

if(DEFINED BIOSIM_LOG_MAX_LEVEL)
  target_compile_definitions(core PUBLIC BIOSIM_LOG_MAX_LEVEL=${BIOSIM_LOG_MAX_LEVEL})
endif()

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
