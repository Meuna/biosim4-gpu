# tomlc17 — vendored in third_party/
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/tomlc17)

include(FetchContent)

if(BIOSIM_BUILD_TESTS)
  FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
  )
  FetchContent_MakeAvailable(unity)
  target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
endif()
