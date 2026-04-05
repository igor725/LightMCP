#pragma once

#include "third_party/json.hpp"

#include <cstdint>
#include <list>
#include <mutex>
#include <stack>

class MCPContent {
  public:
  void setErrorFlag();

  void pushAnnotations(bool userAttention, bool assistantAttention, float priority, std::string_view lastMod = {});
  void popAnnotations();

  bool addText(std::string_view text);
  bool addImage(std::string_view mime, std::span<uint8_t> data);
  bool addAudio(std::string_view mime, std::span<uint8_t> data);
  bool addStructured(nlohmann::json const&& block);

  nlohmann::json const&& popResult();

  private:
  nlohmann::json Result = nlohmann::json::object({
      {"isError", false},
      {"content", nlohmann::json::array()},
  });

  std::stack<nlohmann::json> Annotations;
};

class MCPIO {
  private:
  using json  = nlohmann::json;
  using tcall = std::function<void(json const& req, MCPContent& resp)>;

  struct Tool {
    json const  Info;
    tcall const Callback;
  };

  bool Initialized = false;

  std::mutex Mutex;

  std::list<Tool> Tools = {};

  void sendResponse(std::optional<uint64_t> req_id, json const& resp) const;
  void sendProtocolError(std::optional<uint64_t> req_id, int32_t code, std::string_view err) const;
  void sendError(std::optional<uint64_t> req_id, std::string_view err) const;
  void sendNotification(std::string_view noti) const;

  auto findTool(std::string_view toolName) {
    auto it = Tools.begin();
    for (; it != Tools.end(); ++it) {
      if (it->Info["name"].get_ref<std::string const&>() == toolName) break;
    }

    return it;
  }

  bool makeStep(std::string_view input);

  public:
  bool registerTool(json const& toolDesc, tcall callback);
  bool unregisterTool(std::string_view name);

  void startLoop();
};
