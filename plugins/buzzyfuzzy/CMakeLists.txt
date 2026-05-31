cmake_minimum_required(VERSION 4.1)

include(FetchContent)

FetchContent_Declare(rapidfuzz
  GIT_REPOSITORY https://github.com/rapidfuzz/rapidfuzz-cpp.git
  GIT_TAG main)
FetchContent_MakeAvailable(rapidfuzz)

add_library(LightMCP_BuzzyFuzzy_Plugin SHARED
  plugin.cpp
  core.cpp
)

target_link_libraries(LightMCP_BuzzyFuzzy_Plugin PRIVATE rapidfuzz::rapidfuzz)

set_target_properties(LightMCP_BuzzyFuzzy_Plugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
