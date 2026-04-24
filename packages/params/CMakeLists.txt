find_package(Argtable3 CONFIG REQUIRED)

add_library(params STATIC
  src/params.c
  src/cli.c
  src/toml.c
)
target_compile_features(params PUBLIC c_std_11)
target_include_directories(params PUBLIC include)
target_link_libraries(params
  PUBLIC  core
  PRIVATE argtable3::argtable3 tomlc17
)

if(BIOSIM_BUILD_TESTS)
  add_subdirectory(tests)
endif()
