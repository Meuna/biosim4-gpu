add_library(tomlc17 STATIC tomlc17.c)
target_include_directories(tomlc17 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(tomlc17 PRIVATE c_std_11)
