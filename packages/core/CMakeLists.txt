add_library(core STATIC src/core.c)
target_compile_features(core PUBLIC c_std_11)
target_include_directories(core PUBLIC include)
