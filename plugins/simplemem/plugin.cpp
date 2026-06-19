#include "../../plugin_api.hh"
#include "core.hh"

extern "C" {
void LMCP_RegisterStuff(std::shared_ptr<IMCPIO> server) {
  static auto mem = createMemory();

  server->registerTool(
      {
          {"name", "get_mem_list"},
          {"title", "List all memories"},
          {"description", "This tool returns the list of all previously saved memories. Could be useful for getting missing contextual information."},
          {
              "inputSchema",
              {
                  {"type", "object"},
                  {"additionalProperties", false},
              },
          },
          {"outputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {
                        "list",
                        {
                            {"type", "array"},
                            {"items",
                             {
                                 {"type", "string"},
                                 {"description", "The name this memory can be retrieved with."},
                                 {"additionalProperties", false},
                             }},
                        },
                    },
                }},
               {"additionalProperties", false},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(mem->getList()); });

  server->registerTool(
      {
          {"name", "put_mem"},
          {"title", "Insert a memory"},
          {"description", "This tool puts a memory into the memory list."},
          {
              "inputSchema",
              {
                  {"type", "object"},
                  {"properties",
                   {
                       {
                           "name",
                           {
                               {"type", "string"},
                               {"description", "The name this memory can be retrieved with."},
                           },
                       },
                       {
                           "contents",
                           {
                               {"type", "string"},
                               {"description", "The contents of this memory."},
                           },
                       },
                   }},
                  {"additionalProperties", false},
              },
          },
          {"outputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {
                        "result",
                        {
                            {"type", "boolean"},
                        },
                    },
                }},
               {"additionalProperties", false},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(mem->addMem(req)); });

  server->registerTool(
      {
          {"name", "get_mem"},
          {"title", "Get a memory by name"},
          {"description", "This tool gets a memory from the memory list."},
          {
              "inputSchema",
              {
                  {"type", "object"},
                  {"properties",
                   {
                       {
                           "name",
                           {
                               {"type", "string"},
                               {"description", "The name of memory to be retrieved."},
                           },
                       },
                   }},
                  {"additionalProperties", false},
              },
          },
          {"outputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {
                        "result",
                        {
                            {"type", "string"},
                            {"description", "The contents of memory requested memory (if any)."},
                        },
                    },
                    {
                        "found",
                        {
                            {"type", "boolean"},
                        },
                    },
                }},
               {"additionalProperties", false},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(mem->getMem(req)); });

  server->registerTool(
      {
          {"name", "del_mem"},
          {"title", "Delete a memory"},
          {"description", "This tool removes a memory from the memory list."},
          {
              "inputSchema",
              {
                  {"type", "object"},
                  {"properties",
                   {
                       {
                           "name",
                           {
                               {"type", "string"},
                               {"description", "The name of the memory to be removed."},
                           },
                       },
                   }},
                  {"additionalProperties", false},
              },
          },
          {"outputSchema",
           {
               {"type", "object"},
               {"properties",
                {
                    {
                        "found",
                        {
                            {"type", "boolean"},
                        },
                    },
                }},
               {"additionalProperties", false},
           }},
      },
      [](nlohmann::json const& req, MCPContent& resp) { resp.addStructured(mem->delMem(req)); });
}
}
