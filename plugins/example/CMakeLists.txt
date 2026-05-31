cmake_minimum_required(VERSION 4.1)

add_library(LightMCP_Example_Plugin SHARED
  plugin.cpp
)

set_target_properties(LightMCP_Example_Plugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
