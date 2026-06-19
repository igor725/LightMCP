cmake_minimum_required(VERSION 4.1)

add_library(LightMCP_SimpleMem_Plugin SHARED
  plugin.cpp
  core.cpp
)

set_target_properties(LightMCP_SimpleMem_Plugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
