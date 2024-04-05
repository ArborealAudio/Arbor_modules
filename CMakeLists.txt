cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(xsimd
  GIT_REPOSITORY https://github.com/xtensor-stack/xsimd
  GIT_TAG 12.1.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(xsimd)

FetchContent_Declare(clap_juce_extensions
  GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
  GIT_TAG 8d0754f5d6ca1e95bc207b7743c04ebd7dc17e88
)
FetchContent_MakeAvailable(clap_juce_extensions)

