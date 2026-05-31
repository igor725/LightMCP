cmake_minimum_required(VERSION 4.1)

include(FetchContent)

project(LightMCP VERSION 1.0 LANGUAGES CXX)

set(LMCP_ENABLED_PLUGINS "lua" CACHE STRING "List of enabled plugins (has no effect when compiled as library)")
option(LMCP_STATIC "Compile LightMCP as static library" OFF)

set(CMAKE_CXX_STANDARD 23)

if(LMCP_STATIC)
  add_library(LightMCP STATIC mcp_io.cpp)
else()
  if(LINUX)
    set(LMCP_PLUGIN_SRC plugin_linux.cpp)
  elseif(WIN32)
    set(LMCP_PLUGIN_SRC plugin_win.cpp)
  else()
    set(LMCP_PLUGIN_SRC plugin_stub.cpp)
  endif()

  add_executable(LightMCP
    main.cpp
    mcp_io.cpp
    ${LMCP_PLUGIN_SRC}
  )

  foreach(plugin IN LISTS LMCP_ENABLED_PLUGINS)
    add_subdirectory(plugins/${plugin})
  endforeach()
endif()

include(vscode_launch.cmake)
