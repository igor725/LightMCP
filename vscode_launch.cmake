set(LAUNCH_JSON_DBG_TYPE "cppdbg")
set(LAUNCH_JSON_DBG_PRGEX "")
set(LAUNCH_JSON_DBG_ATT_SETUP "")
set(LAUNCH_JSON_DBG_ATT_ADDPR "")
set(LAUNCH_JSON_DBG_RUN_SETUP "")
set(LAUNCH_JSON_DBG_RUN_ADDPR "")
set(LAUNCH_JSON_DBG_RUN_ENVIR "")

if(WIN32)
  set(LAUNCH_JSON_DBG_TYPE "cppvsdbg")
  set(LAUNCH_JSON_DBG_PRGEX ".exe")
  set(LAUNCH_JSON_DBG_RUN_ADDPR
    "\n\
      \"console\": \"integratedTerminal\",\n\
      \"symbolSearchPath\": \"${workspaceRoot}/build/\","
  )
  set(LAUNCH_JSON_DBG_ATT_ADDPR
    "\n\
      \"symbolSearchPath\": \"${workspaceRoot}/build/\","
  )
elseif(LINUX)
  set(LAUNCH_JSON_DBG_ATT_SETUP
    "\n\
        {\n\
          \"description\": \"Enable pretty-printing for gdb\",\n\
          \"text\": \"-enable-pretty-printing\",\n\
          \"ignoreFailures\": true\n\
        },
        {\n\
          \"description\": \"Set Disassembly Flavor to Intel\",\n\
          \"text\": \"-gdb-set disassembly-flavor intel\",\n\
          \"ignoreFailures\": true\n\
        },
        {\n\
          \"description\": \"Ignore segfaults, they're handled by our exception handler\",\n\
          \"text\": \"handle SIGSEGV nostop pass\",\n\
        },
        {\n\
          \"description\": \"Ignore SIGUSR1s, they're handled by our exception handler\",\n\
          \"text\": \"handle SIGUSR1 nostop pass\",\n\
        },\n  \
    ")
  set(LAUNCH_JSON_DBG_RUN_SETUP
    "\n\
        {\n\
          \"description\": \"Enable pretty-printing for gdb\",\n\
          \"text\": \"-enable-pretty-printing\",\n\
          \"ignoreFailures\": true\n\
        },\n\
        {\n\
          \"description\": \"Set Disassembly Flavor to Intel\",\n\
          \"text\": \"-gdb-set disassembly-flavor intel\",\n\
          \"ignoreFailures\": true\n\
        },\n  \
    "
  )
  set(LAUNCH_JSON_DBG_RUN_ENVIR
    "\n\
          {\n\
            \"name\": \"WAYLAND_DISPLAY\",\n\
            \"value\": \"wayland-0\",\n\
          },\n\
          {\n\
            \"name\": \"DISPLAY\",\n\
            \"value\": \":0\",\n\
          },\n      "
  )

  set(LAUNCH_JSON_DBG_RUN_ADDPR "\n      \"externalConsole\": false,")
endif()

configure_file("${CMAKE_SOURCE_DIR}/.vscode/launch.json.in" "${CMAKE_SOURCE_DIR}/.vscode/launch.json" @ONLY)

unset(LAUNCH_JSON_DBG_TYPE)
unset(LAUNCH_JSON_DBG_PRGEX)
unset(LAUNCH_JSON_DBG_ATT_SETUP)
unset(LAUNCH_JSON_DBG_ATT_ADDPR)
unset(LAUNCH_JSON_DBG_RUN_SETUP)
unset(LAUNCH_JSON_DBG_RUN_ADDPR)
unset(LAUNCH_JSON_DBG_RUN_ENVIR)
