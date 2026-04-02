#pragma once

#include "json.hpp"

#include <cstdint>
#include <list>
#include <mutex>

enum class MCPHook {
  OnServerInit,
  OnToolDiscovery,
};

class MCPIO {
  private:
  using json = nlohmann::json;

  struct Tool {
    json const Info;

    std::function<json(json const& req)> const Callback;
  };

  bool Initialized = false;

  std::mutex Mutex;

  std::list<Tool> Tools = {};

  void sendResponse(std::optional<uint64_t> req_id, json const& resp) const;
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
  bool registerTool(json const& toolDesc, std::function<json(json const& req)> callback);
  bool unregisterTool(std::string const& name);

  void startLoop();
};
