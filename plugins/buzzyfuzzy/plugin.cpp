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
          {"title", "List Projects"},
          {"description", "Lists all AIDoc projects."},
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
          {"title", "Fuzzy Search"},
          {"description", "Fuzzy search project TOC."},
          {"inputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {"project",
                     {
                         {"type", "string"},
                         {"description", "Project name from `aidoc_prj_list`."},
                     }},
                    {"query",
                     {
                         {"type", "string"},
                         {"description", "Search query."},
                     }},
                    {"minScore",
                     {
                         {"type", "number"},
                         {"description", "Minimum match ratio (default 30.0)."},
                     }},
                    {"maxResults",
                     {
                         {"type", "number"},
                         {"description", "Max results (0=unlimited)."},
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
                                           {"description", "Matched title."},
                                       }},
                                      {"filename",
                                       {
                                           {"type", "string"},
                                           {"description", "File path for `aidoc_file_open`."},
                                       }},
                                      {"size",
                                       {
                                           {"type", "number"},
                                           {"description", "File size."},
                                       }},
                                      {"score",
                                       {
                                           {"type", "number"},
                                           {"minimum", 0.0},
                                           {"maximum", 100.0},
                                           {"description", "Fuzzy match score."},
                                       }},
                                  }},
                                 {"additionalProperties", false},
                             }},
                        }

                       },
                       {"error",
                        {
                            {"type", "string"},
                            {"description", "Error (if any)."},
                        }},
                   },
               },
               {"required", nlohmann::json::array({"results"})},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(searcher->searchFuzzy(req)); });

  server->registerTool(
      {
          {"name", "aidoc_prj_contains"},
          {"title", "Substring Search"},
          {"description", "Case-sensitive substring search project TOC."},
          {"inputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {"project",
                     {
                         {"type", "string"},
                         {"description", "Project name from `aidoc_prj_list`."},
                     }},
                    {"query",
                     {
                         {"type", "string"},
                         {"description", "Search query."},
                     }},
                    {"maxResults",
                     {
                         {"type", "number"},
                         {"description", "Max results (0=unlimited)."},
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
                                           {"description", "Matched title."},
                                       }},
                                      {"filename",
                                       {
                                           {"type", "string"},
                                           {"description", "File path for `aidoc_file_open`."},
                                       }},
                                      {"size",
                                       {
                                           {"type", "number"},
                                           {"description", "File size."},
                                       }},
                                  }},
                                 {"additionalProperties", false},
                             }},
                        }

                       },
                       {"error",
                        {
                            {"type", "string"},
                            {"description", "Error (if any)."},
                        }},
                   },
               },
               {"required", nlohmann::json::array({"results"})},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(searcher->searchSubstring(req)); });

  server->registerTool(
      {
          {"name", "aidoc_file_open"},
          {"title", "Open File"},
          {"description", "Get file contents. For relative paths, prepend parent directory. Do not report markdown file links to user."},
          {"inputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {"project",
                     {
                         {"type", "string"},
                         {"description", "Project name from `aidoc_prj_list`."},
                     }},
                    {"filename",
                     {
                         {"type", "string"},
                         {"description", "Filename from fuzzy/contains search."},
                     }},
                    {"filter",
                     {
                         {"type", "array"},
                         {"description", "Optional line filters. Returns only lines containing these strings. Use only when certain (e.g., for TOC)."},
                     }},
                }},
               {"required", nlohmann::json::array({"project", "filename"})},
               {"additionalProperties", false},
           }},
          {"outputSchema",
           {
               {"type", "object"},
               {
                   "properties",
                   {
                       {"contents",
                        {
                            {"type", "string"},
                            {"description", "File contents."},
                        }},
                       {"error",
                        {
                            {"type", "string"},
                            {"description", "Error (if any)."},
                        }},
                   },
               },
               {"additionalProperties", false},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(searcher->openFile(req)); });
}

void LMCP_PrintUsage(std::ostream& out) {
  out << "  --bf-root <path> - Path to the directory with decompiled CHM projects\n";
}
}
