cmake_minimum_required(VERSION 4.1)

include(FetchContent)
include(lua.cmake)

project(LightMCP VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)

add_executable(LightMCP
  main.cpp
  mcp_io.cpp
)

LinkLua("D:/lua-5.5.0/src" LightMCP "")
