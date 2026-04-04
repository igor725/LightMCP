#include "mcp_io.h"

#include <iostream>
#include <string>

void MCPIO::sendResponse(std::optional<uint64_t> req_id, nlohmann::json const& resp) const {
  if (!Initialized || !std::cout.good()) return;

  std::cout << nlohmann::json({
                   {"jsonrpc", "2.0"},
                   {"id", req_id},
                   {"result", resp},
               })
            << std::endl;
}

void MCPIO::sendProtocolError(std::optional<uint64_t> req_id, int32_t code, std::string const& err) const {
  if (!Initialized || !std::cout.good() || !req_id.has_value()) return;

  std::cout << nlohmann::json {{"jsonrpc", "2.0"},
                               {"id", *req_id},
                               {
                                   "error",
                                   {{"code", code}, {"message", err}},
                               }}
            << std::endl;
}

void MCPIO::sendError(std::optional<uint64_t> req_id, std::string const& err) const {
  return sendResponse(req_id, {
                                  {"content", {{{"type", "text"}, {"text", err}}}},
                                  {"isError", true},
                              });
}

void MCPIO::sendNotification(std::string const& noti) const {
  if (!Initialized || !std::cout.good()) return;
  // TODO: `noti` name check

  std::cout << nlohmann::json({
                   {"jsonrpc", "2.0"},
                   {"method", "notifications/" + noti},
               })
            << std::endl;
}

bool MCPIO::registerTool(nlohmann::json const& toolDesc, std::function<bool(json const& req, json& resp)> callback) {
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

  if (findTool(name) != Tools.end()) return false; // Duplicate check

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
  if (input.empty() || !input.starts_with('{')) return false;

  std::lock_guard const lock(Mutex);

  try {
    auto const request = nlohmann::json::parse(input);

    if (!request.is_object()) {
      std::cerr << "Incorrect MCP request" << std::endl;
      return true;
    }

    auto const respId = request.contains("id") ? request["id"].get<uint64_t>() : std::make_optional<uint64_t>();
    if (auto const jmeth = request["method"]; jmeth.is_string()) {
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
        if (auto pit = request.find("params"); pit != request.end() && pit->is_object()) {
          if (auto nit = pit->find("name"), ait = pit->find("arguments"); nit != pit->end() && ait != pit->end()) {
            auto const toolName = nit->get_ref<std::string const&>();
            auto const tool     = findTool(toolName);
            if (tool == Tools.end()) {
              sendProtocolError(respId, -32601, "Unknown tool: " + toolName);
              return true;
            }

            try {
              json content = nlohmann::json::array();

              auto const isError = tool->Callback(*ait, content);

              // TODO: add support for audio and image
              json response = {
                  {"isError", isError},
                  {"content",
                   {
                       {
                           {"type", "text"},
                           {"text", content.dump()},
                       },
                   }},
              };

              if (content.is_structured()) response["structuredContent"] = std::move(content);
              sendResponse(respId, response);
            } catch (std::exception const& ex) {
              sendError(respId, "Tool exception: " + std::string(ex.what()));
            }
          } else {
            sendProtocolError(respId, -32602, "Missing tool name or arguments object");
            return true;
          }
        } else {
          sendProtocolError(respId, -32602, "Invalid params");
          return true;
        }
      } else if (auto const& noti = jmeth.get_ref<std::string const&>(); noti.starts_with("notifications/")) {
        std::cerr << ">> Notification received from client: " << noti << std::endl;
        // TODO: handle notifications?
      } else if (respId.has_value()) {
        sendError(respId, "LightMCP panic: Unhandled MCP method");
        return true;
      }
    } else {
      sendProtocolError(respId, -32602, "No method specified in the request: " + input);
      return false;
    }
  } catch (nlohmann::json::parse_error const& ex) {
    std::cerr << "JSON parsing error: " + input << std::endl;
    return true;
  }

  return true;
}

void MCPIO::startLoop() {
  if (Initialized) return;

  std::string line;

  std::setvbuf(stdout, nullptr, _IONBF, 0);
  while (std::getline(std::cin, line)) {
    if (!makeStep(line)) break;
  }
}
