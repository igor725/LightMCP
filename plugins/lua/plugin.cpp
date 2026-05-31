#include "../../plugin_api.hh"
#include "lua_safe.hh"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#define MCPIO_VM_TIMEOUT_DEF 6
#define MCPIO_PRINT_MAX_DEF  65536
#define MCPIO_RETURN_MAX_DEF 65536

static uint32_t vmTimeout = MCPIO_VM_TIMEOUT_DEF, printMax = MCPIO_PRINT_MAX_DEF, retMax = MCPIO_RETURN_MAX_DEF;

extern "C" {
bool LMCP_HandleArgument(std::string_view arg, std::stringstream& value) {
  if (arg == "--vm-timeout") {
    value >> vmTimeout;
  } else if (arg == "--print-max") {
    value >> printMax;
  } else if (arg == "--return-max") {
    value >> retMax;
  } else {
    return false;
  }

  return true;
}

void LMCP_RegisterStuff(std::shared_ptr<IMCPIO> server) {
  server->registerTool(
      {
          {"name", "run_lua"},
          {"title", "Lua script interpreter"},
          {"description", "This tool executes arbitrary Lua code in a user environment, ideal for math computations and string operations. "
                          "The script runtime duration is limited to prevent infinite loops. "
#ifndef LMCP_UNSAFE
                          "For security, some unsafe functionality in modules like `os` and `io` was stripped to prevent the system damage. "
                          "Using harmless functions like os.time(), os.date() etc is allowed and encouraged if they are needed for a task you solving. "
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
      [](nlohmann::json const& req, std::shared_ptr<MCPContent> resp) {
        enum VMState {
          eNone,
          eRequestParsed = 1 << 0,
          eInitialized   = 1 << 1,
          eCodeCompiled  = 1 << 2,
          eCodeExecuted  = 1 << 3,

          eComplete = eRequestParsed | eInitialized | eCodeCompiled | eCodeExecuted,
        };

        uint32_t    vmState  = eNone;
        uint32_t    nResults = 1; // It's 1 initially for error handling
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

#if LUA_VERSION_NUM > 501
              {LUA_COLIBNAME, luaopen_coroutine},
#endif

#ifdef LUA_UTF8LIBNAME
              {LUA_UTF8LIBNAME, luaopen_utf8},
#endif

#ifdef LUA_JITLIBNAME
              {LUA_JITLIBNAME, luaopen_jit},
#endif

#ifdef LUA_BITLIBNAME
#if LUA_VERSION_NUM == 502
              {LUA_BITLIBNAME, luaopen_bit32},
#elif LUA_VERSION_NUM == 501 // Seems to be a LuaJIT, they have own bitwise op library
              {LUA_BITLIBNAME, luaopen_bit},
#endif
#endif

#ifdef LMCP_UNSAFE
              {LUA_OSLIBNAME, luaopen_os},
              {LUA_IOLIBNAME, luaopen_io},

#ifdef LUA_FFILIBNAME
              {LUA_FFILIBNAME, luaopen_ffi},
#endif
#else
              {LUA_OSLIBNAME, luaopen_os_safe},
              {LUA_IOLIBNAME, luaopen_io_safe},
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
            if (_o->length() > printMax) return luaL_error(L, "Print buffer is longer than %d bytes", printMax);

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

              static clock_t scr_start;

              scr_start = clock();
              lua_sethook(
                  L,
                  [](lua_State* L, lua_Debug* ar) {
                    if ((clock() - scr_start) > (vmTimeout * CLOCKS_PER_SEC)) {
                      luaL_error(L, "VM timeout, script execution took more than %d seconds", vmTimeout);
                    }
                  },
                  LUA_MASKCOUNT, 10000);

              if (auto const execError = lua_pcall(L, 0, LUA_MULTRET, 0); execError == 0) {
                vmState |= eCodeExecuted;
                nResults = lua_gettop(L) - preCall;
              }

              lua_sethook(L, NULL, 0, 0);
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
              if (retString.length() >= retMax) {
              rs_trunc:
                retString.append("[WARNING! return statement is truncated, it's longer than " + std::to_string(retMax) + " bytes]");
                break;
              }

#if LUA_VERSION_NUM < 502
              lua_getglobal(L, "tostring");
              lua_pushvalue(L, i);
              lua_call(L, 1, 1);
#else
              luaL_tolstring(L, i, nullptr);
#endif
              auto const str = std::string_view(lua_tostring(L, -1));
              if ((retString.length() + str.length()) > retMax) {
                lua_pop(L, 1);
                goto rs_trunc;
              }
              if (i > 1) retString.push_back('\t');
              retString.append(str);
              lua_pop(L, 1);
            }
          } else if (retString.empty()) {
            retString = "Code execution succeeded with no returned values";
          }
        }
        lua_close(L);

        resp->addStructured({
            {"stage", vmState},
            {"returned", retString},
            {"prints", printString},
        });

        if (vmState != eComplete) resp->setErrorFlag();
      });
}

void LMCP_PrintUsage(std::ostream& out) {
  out << "  --vm-timeout <seconds> - Maximum Lua script execution duration in seconds (default: " << MCPIO_VM_TIMEOUT_DEF << ")\n";
  out << "  --print-max <bytes> - Maximum bytes in Lua tool \"prints\" output field (default: " << MCPIO_PRINT_MAX_DEF << ")\n";
  out << "  --return-max <bytes> - Maximum bytes in Lua tool \"returned\" output field (default: " << MCPIO_RETURN_MAX_DEF << ")\n";
}
}
