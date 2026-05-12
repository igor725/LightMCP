#include "mcp_io.h"

#include "third_party/base64.hpp"

#include <cassert>
#include <format>
#include <iostream>
#include <regex> // TODO: Use something faster maybe?
#include <string>
#include <string_view>

namespace {
/**
 * @todo Use a real third-party parser? They're usually pretty big so idk,
 *       the json library alone is already big as hell, few times bigger
 *       than the whole server... But it is a kind of must while URI
 *       parser is not. Guess I'll just leave the regex here, whatever
 */
bool uri_check(std::string_view uri) {
  if (uri.empty()) return false;
  static const std::regex uri_regex(R"(^([a-zA-Z][a-zA-Z0-9+.-]*:)?(?://([a-zA-Z0-9-._~%!$&'()*+,;=:@[\]]+)?)?(/[^?#]*)?(\?[^#]*)?(#.*)?$)",
                                    std::regex::optimize);
  return std::regex_match(uri.begin(), uri.end(), uri_regex);
}
} // namespace

nlohmann::json MCPAnnotation::createAnnotation(bool userAttention, bool assistantAttention, float priority, Clock::time_point lastMod) const {
  nlohmann::json audience = nlohmann::json::array();
  if (userAttention) audience.push_back("user");
  if (assistantAttention) audience.push_back("assistant");

  nlohmann::json data {
      {"audience", std::move(audience)},
      {"prio", priority},
  };

  if (lastMod != Clock::time_point {}) data.emplace("lastModified", std::format("{:%FT%TZ}", lastMod));

  return std::move(data);
}

void MCPAnnotations::pushAnnotations(nlohmann::json const&& annotation) {
  Annotations.emplace(std::move(annotation));
}

void MCPAnnotations::popAnnotations() {
  assert(!Annotations.empty());
  Annotations.pop();
}

nlohmann::json MCPResource::generateText(bool metaOnly, std::string_view string, nlohmann::json const&& annotation) const {
  nlohmann::json resource {
      {"uri", URI},
  };
  if (!MimeType.empty()) resource["mimeType"] = MimeType;

  if (metaOnly) {
    if (FileName.empty()) {
      resource["name"] = URI.substr(URI.find_last_of('/') + 1);
    } else {
      resource["name"] = FileName;
    }
    if (!Title.empty()) resource["title"] = Title;
    if (!Description.empty()) resource["description"] = Description;
    resource["size"] = string.size();
    return resource;
  }

  resource["text"] = string;
  return resource;
}

nlohmann::json MCPResource::generateBlob(bool metaOnly, std::span<uint8_t> data, nlohmann::json const&& annotation) const {
  nlohmann::json resource {
      {"uri", URI},
  };
  if (!MimeType.empty()) resource["mimeType"] = MimeType;

  if (metaOnly) {
    if (!Title.empty()) resource["title"] = Title;
    if (!Description.empty()) resource["description"] = Description;
    resource["size"] = data.size_bytes();
    return resource;
  }

  resource["blob"] = base64::encode_into<std::string>(data.begin(), data.end());
  return resource;
}

bool MCPResources::add(nlohmann::json&& resource) {
  Result.push_back(std::move(resource));
  return true;
}

nlohmann::json MCPResources::popResult() {
  return std::move(Result);
}

void MCPContent::setErrorFlag() {
  Result["isError"] = true;
}

bool MCPContent::addText(std::string_view text) {
  nlohmann::json resource = {
      {"type", "text"},
      {"text", text},
  };

  if (!Annotations.empty()) resource["annotation"] = Annotations.top();
  Result["content"].emplace_back(std::move(resource));
  return true;
}

bool MCPContent::addImage(std::string_view mime, std::span<uint8_t> data) {
  if (data.empty()) return false;

  nlohmann::json resource {
      {"type", "image"},
      {"data", base64::encode_into<std::string>(data.begin(), data.end())},
      {"mimeType", mime},
  };

  if (!Annotations.empty()) resource["annotation"] = Annotations.top();
  Result["content"].emplace_back(std::move(resource));
  return true;
}

bool MCPContent::addAudio(std::string_view mime, std::span<uint8_t> data) {
  if (data.empty()) return false;

  nlohmann::json resource {
      {"type", "audio"},
      {"data", base64::encode_into<std::string>(data.begin(), data.end())},
      {"mimeType", mime},
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

nlohmann::json MCPContent::popResult() {
  (std::stack<nlohmann::json> {}).swap(Annotations);
  return std::move(Result);
}

void MCPIO::sendResponse(std::optional<uint64_t> req_id, nlohmann::json const&& resp) const {
  if (!Initialized || !std::cout.good()) return;

  auto const resp1 = nlohmann::json({
                                        {"jsonrpc", "2.0"},
                                        {"id", req_id},
                                        {"result", std::move(resp)},
                                    })
                         .dump();

  std::cout << resp1 << std::endl;
}

void MCPIO::sendProtocolError(std::optional<uint64_t> req_id, int32_t code, std::string_view err, json const&& data) const {
  if (!Initialized || !std::cout.good() || !req_id.has_value()) return;

  nlohmann::json error {
      {"jsonrpc", "2.0"},
      {"id", *req_id},
      {
          "error",
          {
              {"code", code},
              {"message", err},
          },
      },
  };
  if (data.is_object()) error["error"]["data"] = std::move(data);

  std::cout << error << std::endl;
}

void MCPIO::sendError(std::optional<uint64_t> req_id, std::string_view err) const {
  return sendResponse(req_id, {
                                  {"content", {{{"type", "text"}, {"text", err}}}},
                                  {"isError", true},
                              });
}

void MCPIO::sendNotification(std::string_view noti) const {
  if (!Initialized || !std::cout.good()) return;
  // TODO: `noti` name check

  std::cout << nlohmann::json({
                   {"jsonrpc", "2.0"},
                   {"method", "notifications/" + std::string(noti)},
               })
            << std::endl;
}

bool MCPIO::registerTool(nlohmann::json const&& toolDesc, tcall callback) {
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

  Tools.emplace_back(std::move(toolDesc), callback);
  sendNotification("tools/list_changed");
  return true;
}

bool MCPIO::unregisterTool(std::string_view name) {
  std::lock_guard const lock(Mutex);

  if (auto it = findTool(name); it != Tools.end()) {
    Tools.erase(it);
    sendNotification("tools/list_changed");
    return true;
  }

  return false;
}

bool MCPIO::registerResource(std::string_view uri, MCPResource::cbfunc callback, std::string_view name, std::string_view title, std::string_view desc,
                             std::string_view mime) {
  std::lock_guard const lock(Mutex);

  if (findResource(uri) == Resources.end()) {
    Resources.emplace_back(uri, name, title, desc, mime, callback);
    sendNotification("resources/list_changed");
    return true;
  }

  return false;
}

bool MCPIO::unregisterResource(std::string_view uri) {
  std::lock_guard const lock(Mutex);

  if (auto it = findResource(uri); it != Resources.end()) {
    Resources.erase(it);
    sendNotification("resources/list_changed");
    return true;
  }

  return false;
}

bool MCPIO::makeStep(std::string_view input) {
  std::lock_guard const lock(Mutex);

  try {
    auto const request = nlohmann::json::parse(input);

    if (!request.is_object()) {
      std::cerr << "Incorrect MCP request: " << request << std::endl;
      return false;
    }

    auto const respId = request.contains("id") ? request["id"].get<uint64_t>() : std::make_optional<uint64_t>();
    if (auto const& jmeth = request["method"]; jmeth.is_string()) {
      if (jmeth == "initialize") {
        if (auto pit = request.find("params"); pit != request.end() && pit->is_object()) {
          if (auto cprit = pit->find("protocolVersion"); cprit != pit->end() && cprit->is_string()) {
            std::cerr << "Client's protocol: " << cprit->get_ref<std::string const&>() << std::endl;

            auto const init = nlohmann::json({
                {"protocolVersion", "2025-06-18"},
                {"capabilities",
                 {
                     {"tools",
                      {
                          {"listChanged", true},
                      }},
                     {"resources",
                      {
                          {"listChanged", true},
                          {"subscribe", false}, // TODO: Implement resource subscription
                      }},
                 }},
                {"serverInfo",
                 {
                     {"name", "LightMCP"},
                     {"title", "Lightweight MCP server written in C++"},
                     {"version", "1.0.0"},
                 }},
            });

            Initialized = true;
            sendResponse(respId, std::move(init));
          } else {
            sendProtocolError(respId, -32602, "No protocol version specified");
            return false;
          }
        } else {
          sendProtocolError(respId, -32602, "Invalid params");
          return false;
        }

        return true;
      } else if (!Initialized) {
        std::cerr << "Non-init request received in uninitialized state: " << request << std::endl;
        return false;
      }

      if (auto& method = jmeth.get_ref<std::string const&>(); !method.empty()) {
        auto const submethod = std::string_view(method).substr(method.find_first_of('/') + 1);

        if (method.starts_with("tools/")) {
          if (submethod == "list") {
            auto toolsList = nlohmann::json::array();

            for (auto const& tool: Tools) {
              toolsList.push_back(tool.Info);
            }

            sendResponse(respId, {{"tools", std::move(toolsList)}});
          } else if (submethod == "call") {
            if (auto pit = request.find("params"); pit != request.end() && pit->is_object()) {
              if (auto nit = pit->find("name"), ait = pit->find("arguments"); nit != pit->end() && ait != pit->end()) {
                auto& toolName = nit->get_ref<std::string const&>();

                if (auto const tool = findTool(toolName); tool == Tools.end()) {
                  sendProtocolError(respId, -32601, "Unknown tool: " + toolName);
                  return true;
                } else {
                  try {
                    MCPContent content;
                    tool->Callback(*ait, content);
                    sendResponse(respId, content.popResult());
                  } catch (std::exception const& ex) {
                    sendError(respId, "Tool exception: " + std::string(ex.what()));
                  }
                }
              } else {
                sendProtocolError(respId, -32602, "Missing tool name or arguments object");
              }
            } else {
              sendProtocolError(respId, -32602, "Invalid params");
            }
          } else {
            goto unhandled_method;
          }
        } else if (method.starts_with("resources/")) {
          if (submethod == "list") {
            MCPResources resList;

            try {
              for (auto const& res: Resources) {
                resList.add(res.Callback(true, nullptr, res));
              }

              sendResponse(respId, {{"resources", resList.popResult()}});
            } catch (std::exception const& ex) {
              sendError(respId, "Resource list exception: " + std::string(ex.what()));
            }
          } else if (submethod == "read") {
            if (auto pit = request.find("params"); pit != request.end() && pit->is_object()) {
              if (auto uit = pit->find("uri"); uit != pit->end() && uit->is_string()) {
                MCPResources resList;

                try {
                  bool found = false;

                  for (auto const& res: Resources) {
                    if (*uit == res.URI) {
                      resList.add(res.Callback(false, *pit, res));
                      found = true;
                      break;
                    }
                  }

                  if (found) {
                    sendResponse(respId, {{"contents", resList.popResult()}});
                  } else {
                    sendProtocolError(respId, -32002, "Resource not found");
                  }
                } catch (std::exception const& ex) {
                  sendError(respId, "Resource list exception: " + std::string(ex.what()));
                }
              }
            }
          } else {
            goto unhandled_method;
          }
        } else if (method.starts_with("notifications/")) {
          std::cerr << ">> Notification received from client: " << method << std::endl;
          // TODO: handle notifications?
        } else if (respId.has_value()) {
        unhandled_method:
          sendProtocolError(respId, -32602, "Unhandled MCP method");
          std::cerr << "Unhandled MCP method: " << method << std::endl;
        }
      }
    } else {
      sendProtocolError(respId, -32602, "No method specified in the request: " + std::string(input));
      return false;
    }
  } catch (nlohmann::json::parse_error const& ex) {
    std::cerr << "JSON parsing error: " << input << std::endl;
    return false;
  }

  return true;
}

void MCPIO::startLoop() {
  if (Initialized) return;

  std::string line;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  while (std::getline(std::cin, line)) {
    if (!makeStep(line)) break;
    if (!std::cout.good()) break; // Validate stdout state
  }
}
