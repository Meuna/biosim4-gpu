add_library(core STATIC
  src/core.c
  src/params.c
  src/grid.c
)
target_compile_features(core PUBLIC c_std_11)
target_include_directories(core PUBLIC include)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
