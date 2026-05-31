#include "../../plugin_api.hh"

#define TOSTR_H(SS) #SS
#define TOSTR(SS)   TOSTR_H(SS)

extern "C" {
void LMCP_RegisterStuff(std::shared_ptr<IMCPIO> server) {
  server->registerTool(
      {
          {"name", "get_random_integer"},
          {"title", "Random Integer"},
          {"description", "This tool returns a random decimal integer value e.g. 0, 95, 322, 666. Value range is: [0, " TOSTR(RAND_MAX) "]."},
          {"inputSchema", {{"type", "object"}, {"additionalProperties", false}}},
      },
      [](nlohmann::json const& req, std::shared_ptr<MCPContent> resp) { resp->addText(std::to_string(std::rand())); });

  server->registerResource(
      "file:///ExampleResource.txt",
      [](bool metaOnly, nlohmann::json const& req, MCPResource const& self) -> nlohmann::json {
        static const std::string resource = "Luke, I am your father!";

        return self.generateText(metaOnly, resource);
      },
      {} /* Empty means "extract from URI" */, "Example Resource", "This is a example resource created by LightMCP", "plain/text");
}
}
