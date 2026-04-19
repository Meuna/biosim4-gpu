include(FetchContent)

FetchContent_Declare(tomlc17
  GIT_REPOSITORY https://github.com/cktan/tomlc17.git
  GIT_TAG        R260414
)
FetchContent_MakeAvailable(tomlc17)

if(BIOSIM_BUILD_TESTS)
  FetchContent_Declare(unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
  )
  FetchContent_MakeAvailable(unity)
endif()
