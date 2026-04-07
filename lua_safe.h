#pragma once

extern "C" {
#include <lua.h>
}

int luaopen_os_safe(lua_State* L);
int luaopen_io_safe(lua_State* L);
