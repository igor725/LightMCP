#include "mcp_io.h"

#include <iostream>

#define TOSTR_H(SS) #SS
#define TOSTR(SS)   TOSTR_H(SS)

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

int main() {
  std::cerr << "Starting a MCP server" << std::endl;

  std::srand(std::time(nullptr));

  MCPIO server;

  server.registerTool(
      {
          {"name", "get_random_integer"},
          {"title", "Random Integer"},
          {"description", "This tool returns a random integer value e.g. 0, 95, 322, 666. Value range is: [0, " TOSTR(RAND_MAX) "]."},
          {"inputSchema", {{"type", "object"}, {"additionalProperties", false}}},
      },
      [](nlohmann::json const& req) { return std::make_pair<bool, nlohmann::json>(true, {{{"type", "text"}, {"text", std::to_string(std::rand())}}}); });

  server.registerTool(
      {
          {"name", "run_lua"},
          {"title", "Lua script interpreter"},
          {"description", "This tool executes arbitrary Lua code in a user environment, ideal for math computations and string operations. "
#ifndef LUA_CMAKE_UNSAFE
                          "For security, `os` and `io` modules are disabled to prevent system damage. "
#else
                          "OS-level operations like `os.execute`, `io.open`, ... are available too. "
#endif
                          "Example: `return _VERSION` outputs the Lua interpreter version. Note: tables cannot be printed directly - use a `for` loop with "
                          "`print` to iterate and display contents. Script runs internally via `lua_pcall`."},
          {
              "inputSchema",
              {
                  {"type", "object"},
                  {"properties",
                   {{"code",
                     {
                         {"type", "string"},
                         {"description", "Lua code to execute"},
                     }}}},
              },
          },
      },
      [](nlohmann::json const& req) {
        if (auto code = req.find("code"); code != req.end()) {
          std::string outString;

          auto L = luaL_newstate();

          static const luaL_Reg lualibs[] = {
#if LUA_VERSION_NUM > 504
              {LUA_GNAME, luaopen_base},
#else
              {"", luaopen_base},
#endif

#if LUA_VERSION_NUM > 502
              {LUA_COLIBNAME, luaopen_coroutine},
#endif

#ifdef LUA_UTF8LIBNAME
              {LUA_UTF8LIBNAME, luaopen_utf8},
#endif

              {LUA_LOADLIBNAME, luaopen_package}, {LUA_TABLIBNAME, luaopen_table}, {LUA_STRLIBNAME, luaopen_string},
              {LUA_MATHLIBNAME, luaopen_math},    {LUA_DBLIBNAME, luaopen_debug},  {NULL, NULL},
          };

#if LUA_VERSION_NUM > 501
          for (const luaL_Reg* lib = lualibs; lib->func; lib++) {
            luaL_requiref(L, lib->name, lib->func, 1);
            lua_pop(L, 1);
          }
#else
          for (const luaL_Reg* lib = lualibs; lib->func; lib++) {
            lua_pushcfunction(L, lib->func);
            lua_pushstring(L, lib->name);
            lua_call(L, 1, 0);
          }
#endif

          lua_pushlightuserdata(L, &outString);
          lua_setfield(L, LUA_REGISTRYINDEX, "_cxxoutput");
          lua_pushcfunction(L, [](lua_State* L) -> int32_t {
            auto const nArg = lua_gettop(L);
            lua_getfield(L, LUA_REGISTRYINDEX, "_cxxoutput");
            auto _o = reinterpret_cast<std::string*>(lua_touserdata(L, -1));
            lua_pop(L, 1);

#if LUA_VERSION_NUM < 502
            lua_getglobal(L, "tostring");
            for (int32_t i = 1; i <= nArg; ++i) {
              lua_pushvalue(L, -1);
              lua_pushvalue(L, i);
              lua_call(L, 1, 1);
              _o->append(lua_tostring(L, -1));
              _o->push_back('\t');
              lua_pop(L, 1);
            }
#else
            for (int32_t i = 1; i <= nArg; ++i) {
              luaL_tolstring(L, i, nullptr);
              _o->append(lua_tostring(L, -1));
              _o->push_back('\t');
              lua_pop(L, 1);
            }
#endif
            if (!_o->empty()) _o->back() = '\n';

            return 0;
          });
          lua_setglobal(L, "print");

          if (auto const loadErr = luaL_loadstring(L, code->get_ref<std::string const&>().c_str()); loadErr == 0) {
            if (auto const execError = lua_pcall(L, 0, 1, 0); execError == 0) {
              if (!lua_isnoneornil(L, -1)) {
#if LUA_VERSION_NUM < 502
                lua_getglobal(L, "tostring");
                lua_pushvalue(L, -2);
                lua_call(L, 1, 1);
#else
                luaL_tolstring(L, -1, nullptr);
#endif
                outString.append(lua_tostring(L, -1));
              }

              lua_close(L);
              return std::make_pair<bool, nlohmann::json>(true, {{{"type", "text"}, {"text", outString}}});
            } else {
              outString.push_back('\n');
              outString.append(lua_tostring(L, -1));
              lua_close(L);
              return std::make_pair<bool, nlohmann::json>(false, {{{"type", "text"}, {"text", "Failed to execute `code` block!\n" + outString}}});
            }
          } else {
            outString.push_back('\n');
            outString.append(lua_tostring(L, -1));
            lua_close(L);
            return std::make_pair<bool, nlohmann::json>(false, {{{"type", "text"}, {"text", "Failed to compile `code` block!\n" + outString}}});
          }

          lua_close(L);
        }

        return std::make_pair<bool, nlohmann::json>(false, {{{"type", "text"}, {"text", "Missing `code` block!"}}});
      });

  server.startLoop();

  return 0;
}
