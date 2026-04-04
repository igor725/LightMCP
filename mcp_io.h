#pragma once

#include "json.hpp"

#include <cstdint>
#include <list>
#include <mutex>

class MCPIO {
  private:
  using json  = nlohmann::json;
  using tcall = std::function<bool(json const& req, json& resp)>;

  struct Tool {
    json const  Info;
    tcall const Callback;
  };

  bool Initialized = false;

  std::mutex Mutex;

  std::list<Tool> Tools = {};

  void sendResponse(std::optional<uint64_t> req_id, json const& resp) const;
  void sendProtocolError(std::optional<uint64_t> req_id, int32_t code, std::string const& err) const;
  void sendError(std::optional<uint64_t> req_id, std::string const& err) const;
  void sendNotification(std::string const& noti) const;

  auto findTool(std::string const& toolName) {
    auto it = Tools.begin();
    for (; it != Tools.end(); ++it) {
      if (it->Info["name"] == toolName) break;
    }

    return it;
  }

  bool makeStep(std::string const& input);

  public:
  bool registerTool(json const& toolDesc, tcall callback);
  bool unregisterTool(std::string const& name);

  void startLoop();
};
