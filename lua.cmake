function(LinkLua src target components)
  set(source_list
    ${src}/lapi.c
    ${src}/lauxlib.c
    ${src}/lbaselib.c
    ${src}/lcode.c
    ${src}/ldblib.c
    ${src}/ldebug.c
    ${src}/ldo.c
    ${src}/ldump.c
    ${src}/lfunc.c
    ${src}/lgc.c
    ${src}/llex.c
    ${src}/lmathlib.c
    ${src}/lmem.c
    ${src}/loadlib.c
    ${src}/lobject.c
    ${src}/lopcodes.c
    ${src}/lparser.c
    ${src}/lstate.c
    ${src}/lstring.c
    ${src}/lstrlib.c
    ${src}/ltable.c
    ${src}/ltablib.c
    ${src}/ltm.c
    ${src}/lundump.c
    ${src}/lvm.c
    ${src}/lzio.c
  )

  if("unsafe" IN_LIST components)
    target_compile_definitions(${target} PRIVATE LUA_CMAKE_UNSAFE)
    list(APPEND source_list "${src}/loslib.c;${src}/liolib.c")
  endif()

  add_library(luavm STATIC ${source_list})
  target_link_libraries(${target} PRIVATE luavm)
  target_include_directories(${target} PRIVATE ${src})
endfunction(LinkLua)
