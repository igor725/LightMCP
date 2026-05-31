cmake_minimum_required(VERSION 4.1)

option(LMCP_UNSAFE "Enable unsafe Lua functionality for MCP server" OFF)
set(LMCP_LUA_VERSION "5.1" CACHE STRING "Sets the Lua interpreter version for CMake to find")

add_library(LightMCP_Lua_Plugin SHARED
  plugin.cpp
  lua_safe.cpp
)

set_target_properties(LightMCP_Lua_Plugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

find_package(Lua ${LMCP_LUA_VERSION} EXACT REQUIRED)

target_link_libraries(LightMCP_Lua_Plugin PRIVATE ${LUA_LIBRARIES})
target_include_directories(LightMCP_Lua_Plugin PRIVATE ${LUA_INCLUDE_DIR})

target_compile_definitions(LightMCP_Lua_Plugin PRIVATE $<$<BOOL:${LMCP_UNSAFE}>:LMCP_UNSAFE>)
