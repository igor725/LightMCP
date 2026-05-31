#include "../../plugin_api.hh"
#include "core.hh"

#include <rapidfuzz/fuzz.hpp>

std::filesystem::path md_root;

extern "C" {
bool LMCP_HandleArgument(std::string_view arg, std::stringstream& value) {
  if (arg == "--bf-root") {
    value >> md_root;
  } else {
    return false;
  }

  return true;
}

void LMCP_RegisterStuff(std::shared_ptr<IMCPIO> server) {
  if (md_root.empty()) return;
  static auto searcher = createSearch(md_root);

  server->registerTool(
      {
          {"name", "aidoc_prj_list"},
          {"title", "AIDoc projects list"},
          {"description", "Returns the list of AIDoc projects."},
          {"inputSchema", {{"type", "object"}, {"additionalProperties", false}}},
          {"outputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {
                        "list",
                        {
                            {"type", "array"},
                            {"items", {{"type", "string"}}},
                        },
                    },
                }},
               {"additionalProperties", false},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(searcher->getList()); });

  server->registerTool(
      {
          {"name", "aidoc_prj_fuzzy"},
          {"title", "Fuzzy AIDoc Search"},
          {"description", "Fuzzy search through AIDoc project's table of contents."},
          {"inputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {"project",
                     {
                         {"type", "string"},
                         {"description", "The project name received from `aidoc_prj_list` tool"},
                     }},
                    {"query",
                     {
                         {"type", "string"},
                         {"description", "Your search queryto the project"},
                     }},
                    {"minScore",
                     {
                         {"type", "number"},
                         {"description", "Minimal fuzzy search matching ration, default value is 40.0"},
                     }},
                    {"maxResults",
                     {
                         {"type", "number"},
                         {"description", "Maximum results to output, default is `-1` (unlimited)"},
                     }},
                }},
               {"required", nlohmann::json::array({"project", "query"})},
               {"additionalProperties", false},
           }},
          {"outputSchema",
           {
               {"type", "object"},
               {
                   "properties",
                   {
                       {"results",
                        {
                            {"type", "array"},
                            {"items",
                             {
                                 {"type", "object"},
                                 {"properties",
                                  {
                                      {"title",
                                       {
                                           {"type", "string"},
                                           {"description", "The title that was matched against the query."},
                                       }},
                                      {"filename",
                                       {
                                           {"type", "string"},
                                           {"description", "The file path that can be used with the tool `aidoc_file_open`."},
                                       }},
                                      {"score",
                                       {
                                           {"type", "number"},
                                           {"minimum", 0.0},
                                           {"maximum", 100.0},
                                           {"description", "The fuzzy matching score for this entry."},
                                       }},
                                  }},
                                 {"additionalProperties", false},
                             }},
                        }

                       },
                       {"error",
                        {
                            {"type", "string"},
                            {"description", "Returns an error (if any)"},
                        }},
                   },
               },
               {"required", nlohmann::json::array({"results"})},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(searcher->searchFuzzy(req)); });

  server->registerTool(
      {
          {"name", "aidoc_file_open"},
          {"title", "Open AIDoc file"},
          {"description", "Get the contents of AIDoc file. Links in markdown files (if any) can be used by this tool, but links should never be reported to "
                          "user. Relative paths are allowed but you should preserve the parent directory and append it before the relative path."},
          {"inputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {"project", {{"type", "string"}}},
                    {"filename", {{"type", "string"}}},
                }},
               {"required", nlohmann::json::array({"project", "query"})},
               {"additionalProperties", false},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(searcher->openFile(req)); });
}

void LMCP_PrintUsage(std::ostream& out) {
  out << "  --bf-root <path> - Path to the directory with decompiled CHM projects\n";
}
}
