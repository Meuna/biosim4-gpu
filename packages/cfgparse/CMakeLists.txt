find_package(Argtable3 CONFIG REQUIRED)

add_library(cfgparse STATIC
  src/barriers.c
  src/challenges.c
  src/cli.c
  src/toml.c
)
target_compile_features(cfgparse PUBLIC c_std_11)
target_include_directories(cfgparse PUBLIC include)
target_link_libraries(cfgparse
  PUBLIC  core
  PRIVATE argtable3::argtable3 tomlc17
)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
