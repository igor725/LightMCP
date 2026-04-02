#include "mcp_io.h"

#include <iostream>
#include <string>

void MCPIO::sendResponse(std::optional<uint64_t> req_id, nlohmann::json const& resp) const {
  if (!Initialized) return;

  std::cout << nlohmann::json({
                   {"jsonrpc", "2.0"},
                   {"id", req_id},
                   {"result", resp},
               })
            << std::endl;
}

void MCPIO::sendNotification(std::string const& noti) const {
  if (!Initialized) return;
  // TODO: `noti` name check

  std::cout << nlohmann::json({
                   {"jsonrpc", "2.0"},
                   {"method", "notifications/" + noti},
               })
            << std::endl;
}

bool MCPIO::registerTool(nlohmann::json const& toolDesc, std::function<json(json const& req)> callback) {
  std::lock_guard const lock(Mutex);

  if (!toolDesc.is_object()) return false;
  if (auto it = toolDesc.find("name"); it == toolDesc.end() || !it->is_string()) return false;
  if (auto it = toolDesc.find("title"); it == toolDesc.end() || !it->is_string()) return false;
  if (auto it = toolDesc.find("description"); it == toolDesc.end() || !it->is_string()) return false;
  if (auto it = toolDesc.find("outputSchema"); it != toolDesc.end() && !it->is_object()) return false;

  if (auto const itsch = toolDesc.find("inputSchema"); itsch == toolDesc.end() || !itsch->is_object()) {
    return false;
  } else {
    if (auto const ittyp = itsch->find("type"); ittyp == itsch->end() || !ittyp->is_string()) return false;
  }

  auto const& name = toolDesc["name"].get_ref<std::string const&>();
  if (name.empty() || name.length() > 128) return false;
  if (!std::ranges::all_of(name, [](char c) { return std::isalnum(c) || c == '_' || c == '-' || c == '.'; })) return false;

  if (findTool(toolDesc["name"]) != Tools.end()) return false; // Duplicate check

  // TODO: more checks

  Tools.emplace_back(toolDesc, callback);
  sendNotification("tools/list_changed");
  return true;
}

bool MCPIO::unregisterTool(std::string const& name) {
  std::lock_guard const lock(Mutex);

  if (auto it = findTool(name); it != Tools.end()) {
    Tools.erase(it);
    sendNotification("tools/list_changed");
    return true;
  }

  return false;
}

bool MCPIO::makeStep(std::string const& input) {
  std::lock_guard const lock(Mutex);

  auto const request = nlohmann::json::parse(input);

  if (!request.is_object()) {
    std::cerr << "Incorrect MCP request" << std::endl;
    return true;
  }

  if (auto const jmeth = request["method"]; jmeth.is_string()) {
    auto const respId = request.contains("id") ? request["id"].get<uint64_t>() : std::make_optional<uint64_t>();

    if (jmeth == "initialize") {
      auto const init = nlohmann::json({
          {"protocolVersion", "2025-06-18"},
          {"capabilities", {{"tools", {{"listChanged", true}}}}},
          {"serverInfo",
           {
               {"name", "LightMCP"},
               {"title", "Lightweight MCP server written in C++"},
               {"version", "1.0.0"},
           }},
      });

      Initialized = true;
      sendResponse(respId, init);
    } else if (jmeth == "tools/list") {
      auto toolsList = nlohmann::json::array();

      for (auto const& tool: Tools) {
        toolsList.push_back(tool.Info);
      }

      sendResponse(respId, {{"tools", std::move(toolsList)}});
    } else if (jmeth == "tools/call") {
      if (auto pit = request.find("params"); pit != request.end()) {
        if (auto nit = pit->find("name"), ait = pit->find("arguments"); nit != pit->end() || ait != pit->end()) {
          auto const tool = findTool(nit->get_ref<std::string const&>());
          if (tool == Tools.end()) {
            // TODO: Handle unknown tool call
            std::cerr << "LMCP Panic: Unknown tool!" << std::endl;
            return false;
          }

          auto const toolResult = tool->Callback(*ait);

          if (!toolResult.is_array()) {
            std::cerr << "LMCP Panic: Tool response is not an array: " << toolResult << std::endl;
            return false;
          }

          sendResponse(respId, {{
                                   "content",
                                   std::move(toolResult),
                               }});
        } else {
          std::cerr << "LMCP Panic: Invalid params: " << *pit << std::endl;
          return false;
        }
      } else {
        std::cerr << "LMCP Panic: Missing params: " << input << std::endl;
        return false;
      }
    } else if (jmeth.get_ref<std::string const&>().starts_with("notifications/")) {
      // TODO handle notifications?
    } else {
      std::cerr << "LMCP Panic: Unknown method: " << jmeth << std::endl;
      return false;
    }
  } else {
    std::cerr << "LMCP Panic: Invalid request: " << input << std::endl;
    return false;
  }

  return true;
}

void MCPIO::startLoop() {
  if (Initialized) return;

  std::string line;

  while (std::getline(std::cin, line)) {
    if (!makeStep(line)) break;
  }
}
