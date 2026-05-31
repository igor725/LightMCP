#include "mcp_io.hh"
#include "plugin_api.hh"

#include <ctime>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
  std::filesystem::path const pluginsDir = std::filesystem::path(argv[0]).parent_path() / "plugins";
  std::srand(std::time(nullptr));

  auto pluginMachine = createPluginMachine(pluginsDir);

  std::stringstream ss;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "--") break;
    ++i; // Skip to argument value
    if (i >= argc) goto usage;
    ss << argv[i];

    if (pluginMachine->handleArgument(arg, ss)) {
      ss.clear();
      continue;
    }

  usage:
    std::cerr << "Usage: " << argv[0] << " [...]\n\nAvailable options:\n";
    pluginMachine->printUsage(std::cerr);
    std::cerr << std::endl;
    return 0;
  }

  std::cerr << "Starting a MCP server" << std::endl;

  auto server = createMCPServer();

  pluginMachine->registerStuff(server);
  server->startLoop();
  pluginMachine.reset();

  return -1; // Should be unreachable normally
}
