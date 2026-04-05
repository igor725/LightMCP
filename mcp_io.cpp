#include "mcp_io.h"

#include "third_party/base64.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

void MCPContent::setErrorFlag() {
  Result["isError"] = true;
}

void MCPContent::pushAnnotations(bool userAttention, bool assistantAttention, float priority, std::string_view lastMod) {
  nlohmann::json audience = nlohmann::json::array();
  if (userAttention) audience.push_back("user");
  if (assistantAttention) audience.push_back("assistant");

  nlohmann::json data {
      {"audience", std::move(audience)},
      {"prio", priority},
  };

  if (!lastMod.empty()) data.emplace("lastModified", lastMod);
  Annotations.emplace(std::move(data));
}

void MCPContent::popAnnotations() {
  assert(!Annotations.empty());
  Annotations.pop();
}

bool MCPContent::addText(std::string const& text) {
  nlohmann::json resource = {
      {"type", "text"},
      {"text", text},
  };

  if (!Annotations.empty()) resource["annotation"] = Annotations.top();
  Result["content"].emplace_back(std::move(resource));
  return true;
}

bool MCPContent::addImage(ImageMimeType type, std::span<uint8_t> data) {
  if (data.empty()) return false;

  auto const mimeString = [&]() -> std::string_view {
    switch (type) {
      case ImageMimeType::ePNG: return "image/png";
      case ImageMimeType::eJPEG: return "image/jpeg";
      default: return "image/unknown";
    }
  };

  nlohmann::json resource {
      {"type", "audio"},
      {"data", base64::encode_into<std::string>(data.begin(), data.end())},
      {"mimeType", mimeString()},
  };

  if (!Annotations.empty()) resource["annotation"] = Annotations.top();
  Result["content"].emplace_back(std::move(resource));
  return true;
}

bool MCPContent::addAudio(AudioMimeType type, std::span<uint8_t> data) {
  if (data.empty()) return false;

  auto const mimeString = [&]() {
    switch (type) {
      case AudioMimeType::eWaveform: return "audio/wav";
      case AudioMimeType::eMP3: return "audio/mp3";
      default: return "audio/unknown";
    }
  };

  nlohmann::json resource {
      {"type", "audio"},
      {"data", base64::encode_into<std::string>(data.begin(), data.end())},
      {"mimeType", mimeString()},
  };

  if (!Annotations.empty()) resource["annotation"] = Annotations.top();
  Result["content"].emplace_back(std::move(resource));
  return true;
}

bool MCPContent::addStructured(nlohmann::json const&& block) {
  if (!block.is_object()) return false;
  if (Result.find("structuredContent") != Result.end()) return false;
  addText(block.dump()); // Backward compatibility?
  Result["structuredContent"] = std::move(block);
  return true;
}

nlohmann::json const&& MCPContent::popResult() {
  return std::move(Result);
}

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

  std::cout << nlohmann::json({
                   {"jsonrpc", "2.0"},
                   {"id", *req_id},
                   {
                       "error",
                       {{"code", code}, {"message", err}},
                   },
               })
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

bool MCPIO::registerTool(nlohmann::json const& toolDesc, tcall callback) {
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
        if (auto pit = request.find("params"); pit != request.end() && pit->is_object()) {
          if (auto cprit = pit->find("protocolVersion"); cprit != pit->end() && cprit->is_string()) {
            std::cerr << "Client's protocol: " << cprit->get_ref<std::string const&>() << std::endl;

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
          } else {
            sendProtocolError(respId, -32602, "No protocol version specified");
          }
        } else {
          sendProtocolError(respId, -32602, "Invalid params");
        }
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
              MCPContent content;
              tool->Callback(*ait, content);
              sendResponse(respId, content.popResult());
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
