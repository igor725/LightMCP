#include "lua_safe.h"

#include <cstring>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

int create_safe(lua_State* L, const char* libname, lua_CFunction open, const char* safeList[]) {
  if (L == nullptr || open == nullptr || safeList == nullptr) return 0;

#if LUA_VERSION_NUM > 501
  luaL_requiref(L, libname, open, 1);
#else
  lua_pushcfunction(L, open);
  lua_pushstring(L, libname);
  lua_call(L, 1, 1);
#endif

  lua_pushnil(L);
  while (lua_next(L, -2)) {
    lua_pop(L, 1); // Pop valuie
    const char* keyPtr = lua_tostring(L, -1);

    bool shouldRemove = true;
    if (!lua_isstring(L, -1)) goto remove_key;

    for (int i = 0; safeList[i]; ++i) {
      if (::strcmp(keyPtr, safeList[i]) == 0) shouldRemove = false;
    }
    if (!shouldRemove) continue; // Key is safe, no need to remove

  remove_key:
    lua_pushnil(L);      // Push nil instead of original value
    lua_settable(L, -3); // Set to table
    lua_pushnil(L);      // Restart the iteration by pushing nil key
  }

  return 1;
}

int luaopen_os_safe(lua_State* L) {
  const char* safeList[] = {"difftime", "time", "date", "clock", nullptr};
  return create_safe(L, LUA_OSLIBNAME, luaopen_os, safeList);
}

int luaopen_io_safe(lua_State* L) {
  const char* safeList[] = {"stderr", "type", "flush", nullptr};
  return create_safe(L, LUA_IOLIBNAME, luaopen_io, safeList);
}
