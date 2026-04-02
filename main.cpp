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

  // server.registerTool(
  //     {
  //         {"name", "get_random_integer"},
  //         {"title", "Random Integer"},
  //         {"description", "This tool returns a random integer value e.g. 0, 95, 322, 666. Value range is: [0, " TOSTR(RAND_MAX) "]."},
  //         {"inputSchema", {{"type", "object"}, {"additionalProperties", false}}},
  //     },
  //     [](nlohmann::json const& req) -> nlohmann::json {
  //       return {{
  //           {"type", "text"},
  //           {"text", std::to_string(std::rand())},
  //       }};
  //     });

  server.registerTool(
      {
          {"name", "run_lua"},
          {"title", "Execute Lua script"},
          {"description",
           "This tool executes specified Lua code in user environment. For example: `return _VERSION` will output the current Lua interpreter version."},
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
      [](nlohmann::json const& req) -> nlohmann::json {
        if (auto code = req.find("code"); code != req.end()) {
          std::string outString = "Output: ";

          auto L = luaL_newstate();

          static const luaL_Reg lualibs[] = {
              {"", luaopen_base},
              {LUA_LOADLIBNAME, luaopen_package},
              {LUA_TABLIBNAME, luaopen_table},
              {LUA_STRLIBNAME, luaopen_string},
              {LUA_MATHLIBNAME, luaopen_math},
              {LUA_DBLIBNAME, luaopen_debug},
              {NULL, NULL},
          };

          for (const luaL_Reg* lib = lualibs; lib->func; lib++) {
            lua_pushcfunction(L, lib->func);
            lua_pushstring(L, lib->name);
            lua_call(L, 1, 0);
          }

          lua_pushlightuserdata(L, &outString);
          lua_setfield(L, LUA_REGISTRYINDEX, "_cxxoutput");
          lua_pushcfunction(L, [](lua_State* L) -> int32_t {
            auto const nArg = lua_gettop(L);
            lua_getfield(L, LUA_REGISTRYINDEX, "_cxxoutput");
            auto _o = reinterpret_cast<std::string*>(lua_touserdata(L, -1));
            lua_pop(L, 1);

            lua_getglobal(L, "tostring");
            for (int32_t i = 1; i <= nArg; ++i) {
              lua_pushvalue(L, -1);
              lua_pushvalue(L, i);
              lua_call(L, 1, 1);
              _o->append(lua_tostring(L, -1));
              _o->push_back('\t');
              lua_pop(L, 1);
            }
            if (!_o->empty()) _o->back() = '\n';

            return 0;
          });
          lua_setfield(L, LUA_GLOBALSINDEX, "print");

          if (auto const loadErr = luaL_loadstring(L, code->get_ref<std::string const&>().c_str()); loadErr == 0) {
            if (auto const execError = lua_pcall(L, 0, 1, 0); execError == 0) {
              if (!lua_isnoneornil(L, -1)) {
                lua_getglobal(L, "tostring");
                lua_pushvalue(L, -2);
                lua_call(L, 1, 1);
                outString.append(lua_tostring(L, -1));
              }

              lua_pop(L, 2);

              lua_close(L);
              return {{
                  {"type", "text"},
                  {"text", outString},
              }};
            } else {
              outString.push_back('\n');
              outString.append(lua_tostring(L, -1));
              lua_close(L);
              return {{{"type", "text"}, {"text", "Error: Failed to execute `code` block!\n" + outString}}};
            }
          } else {
            outString.push_back('\n');
            outString.append(lua_tostring(L, -1));
            lua_close(L);
            return {{{"type", "text"}, {"text", "Error: Failed to compile `code` block!\n" + outString}}};
          }

          lua_close(L);
        }

        return {{{"type", "text"}, {"text", "Error: Missing `code` block!"}}};
      });

  server.startLoop();

  return 0;
}
