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
  std::srand(std::time(nullptr));

  std::cerr << "Starting a MCP server" << std::endl;

  MCPIO server;

  server.registerTool(
      {
          {"name", "get_random_integer"},
          {"title", "Random Integer"},
          {"description", "This tool returns a random decimal integer value e.g. 0, 95, 322, 666. Value range is: [0x0, " TOSTR(RAND_MAX) "]."},
          {"inputSchema", {{"type", "object"}, {"additionalProperties", false}}},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addText(std::to_string(std::rand())); });

  server.registerTool(
      {
          {"name", "run_lua"},
          {"title", "Lua script interpreter"},
          {"description", "This tool executes arbitrary Lua code in a user environment, ideal for math computations and string operations. "
#ifndef LMCP_UNSAFE
                          "For security, `os` and `io` modules are disabled to prevent system damage. "
#else
                          "OS-level operations like `os.execute`, `io.open`, ... are available too. "
#endif
                          "Example: `return _VERSION` outputs the Lua interpreter version. Note: tables cannot be printed directly - use a `for` loop with "
                          "`print()` to iterate and display contents. Script runs internally via `lua_pcall`."},
          {
              "inputSchema",
              {
                  {"type", "object"},
                  {"properties",
                   {
                       {
                           "code",
                           {
                               {"type", "string"},
                               {"description", "Lua code to execute"},
                           },
                       },
                   }},
                  {"required", nlohmann::json::array({"code"})},
              },
          },
          {
              "outputSchema",
              {
                  {"type", "object"},
                  {"properties",
                   {
                       {"stage",
                        {
                            {"type", "number"},
                            {"description", "Code execution stage."
                                            "Values: RequestParsed = 1, Initialized = 3, CodeCompiled = 7, CodeExcuted = 15."
                                            "Everything below 15 means there was an error."},
                        }},
                       {"returned",
                        {
                            {"type", "string"},
                            {"description", "Values passed to top level `return` statement in the `code` block or error string."},
                        }},
                       {"prints",
                        {
                            {"type", "string"},
                            {"description", "Every print call data."},
                        }},
                   }},
                  {"required", nlohmann::json::array({"stage", "returned", "prints"})},
              },
          },
      },
      [](nlohmann::json const& req, MCPContent& resp) {
        enum VMState {
          eNone,
          eRequestParsed = 1 << 0,
          eInitialized   = 1 << 1,
          eCodeCompiled  = 1 << 2,
          eCodeExecuted  = 1 << 3,

          eComplete = eRequestParsed | eInitialized | eCodeCompiled | eCodeExecuted,
        };

        uint32_t    vmState  = eNone;
        uint32_t    nResults = 1;
        std::string retString, printString;
        lua_State*  L;

        auto const setupLua = [&](lua_State* L) {
          if (L == nullptr) return false;

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

#ifdef LUA_JITLIBNAME
              {LUA_JITLIBNAME, luaopen_jit},
#endif

#ifdef LMCP_UNSAFE
              {LUA_OSLIBNAME, luaopen_os},
              {LUA_IOLIBNAME, luaopen_io},

#ifdef LUA_FFILIBNAME
              {LUA_FFILIBNAME, luaopen_ffi},
#endif
#endif

#ifdef LMCP_UNSAFE
              {LUA_LOADLIBNAME, luaopen_package},
#endif

              {LUA_TABLIBNAME, luaopen_table},
              {LUA_STRLIBNAME, luaopen_string},
              {LUA_MATHLIBNAME, luaopen_math},
              {LUA_DBLIBNAME, luaopen_debug},
              {NULL, NULL},
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

#ifndef LMCP_UNSAFE
          const char* unsafe[] = {"require", "loadfile", "dofile", nullptr};
          for (uint32_t i = 0; unsafe[i]; ++i) {
            lua_pushnil(L);
            lua_setglobal(L, unsafe[i]);
          }
#endif

          vmState |= eInitialized;

          lua_pushlightuserdata(L, &printString);
          lua_setfield(L, LUA_REGISTRYINDEX, "_cxxprint");
          lua_pushcfunction(L, [](lua_State* L) -> int32_t {
            auto const nArg = lua_gettop(L);
            lua_getfield(L, LUA_REGISTRYINDEX, "_cxxprint");
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
          return true;
        };

        if (auto const code = req.find("code"); code != req.end()) {
          vmState |= eRequestParsed;

          if (setupLua(L = luaL_newstate())) {
            int32_t const preCall = lua_gettop(L);
            if (auto const loadErr = luaL_loadstring(L, code->get_ref<std::string const&>().c_str()); loadErr == 0) {
              vmState |= eCodeCompiled;

              if (auto const execError = lua_pcall(L, 0, LUA_MULTRET, 0); execError == 0) {
                vmState |= eCodeExecuted;
                nResults = lua_gettop(L) - preCall;
              }
            }
          } else {
            retString = "Failed to initialize LuaVM: Out of memory!";
          }

        } else {
          retString = "Missing `code` block in the call request!";
        }

        if ((vmState & eInitialized) == eInitialized) {
          if ((nResults == 1 && !lua_isnoneornil(L, -1)) || nResults > 1) {
            for (int32_t i = 1; i <= nResults; ++i) {
#if LUA_VERSION_NUM < 502
              lua_getglobal(L, "tostring");
              lua_pushvalue(L, i);
              lua_call(L, 1, 1);
#else
              luaL_tolstring(L, i, nullptr);
#endif
              if (i > 1) retString.push_back('\t');
              retString.append(lua_tostring(L, -1));
              lua_pop(L, 1);
            }
          } else if (retString.empty()) {
            retString = "Code execution succeeded with no returned values";
          }
        }
        lua_close(L);

        resp.addStructured({
            {"stage", vmState},
            {"returned", retString},
            {"prints", printString},
        });

        if (vmState != eComplete) resp.setErrorFlag();
      });

  server.startLoop();
  return -1; // Should be unreachable normally
}
