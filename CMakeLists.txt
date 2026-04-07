cmake_minimum_required(VERSION 4.1)

include(FetchContent)

project(LightMCP VERSION 1.0 LANGUAGES CXX C)

option(LMCP_UNSAFE "Enable unsafe Lua functionality for MCP server (has no effect when compiled as library)" OFF)
set(LMCP_LUA_VERSION "5.1" CACHE STRING "Sets the Lua interpreter version for CMake to find (has no effect when compiled as library)")
option(LCMP_STATIC "Compile LightMCP as static library" OFF)

set(CMAKE_CXX_STANDARD 23)

if(LCMP_STATIC)
  add_library(LightMCP STATIC mcp_io.cpp)
else()
  add_executable(LightMCP
    main.cpp
    mcp_io.cpp
  )

  find_package(Lua ${LMCP_LUA_VERSION} EXACT REQUIRED)

  if(Lua_FOUND AND NOT TARGET Lua::Lua)
    add_library(Lua::Lua INTERFACE IMPORTED)
    set_target_properties(
      Lua::Lua
      PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
    )
  endif()

  target_link_libraries(LightMCP PRIVATE Lua::Lua)

  target_compile_definitions(LightMCP PRIVATE $<$<BOOL:${LMCP_UNSAFE}>:LMCP_UNSAFE>)
endif()
