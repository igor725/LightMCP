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
  using json = nlohmann::json;

  private:
  void sendResponse(std::optional<uint64_t> req_id, json const& resp) const;
  void sendNotification(std::string const& noti) const;

  struct Tool {
    json const Info;

    std::function<json(json const& req)> const Callback;
  };

  bool Initialized = false;

  std::mutex Mutex;

  std::list<Tool> Tools = {};

  bool makeStep(std::string const& input);

  public:
  bool registerTool(json const& toolDesc, std::function<json(json const& req)> callback);
  bool unregisterTool(std::string const& name);

  void startLoop();
};
