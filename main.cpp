#include "mcp_io.h"

#include <iostream>

#define TOSTR_H(SS) #SS
#define TOSTR(SS)   TOSTR_H(SS)

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
      [](nlohmann::json const&) -> nlohmann::json {
        return {{
            {"type", "text"},
            {"text", std::to_string(std::rand())},
        }};
      });

  server.startLoop();

  return 0;
}
