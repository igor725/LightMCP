cmake_minimum_required(VERSION 4.1)

include(FetchContent)

project(TestMCP VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(TestMCP
  main.cpp
  mcp_io.cpp
)
