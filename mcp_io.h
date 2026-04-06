#pragma once

#include "third_party/json.hpp"

#include <cstdint>
#include <list>
#include <mutex>
#include <stack>

class MCPAnnotation {
  public:
  void pushAnnotations(bool userAttention, bool assistantAttention, float priority, std::string_view lastMod = {});
  void popAnnotations();

  protected:
  std::stack<nlohmann::json> Annotations;
};

class MCPResources: public MCPAnnotation {
  using sv = std::string_view;

  public:
  MCPResources(bool metaOnly): IsMeta(metaOnly) {}

  inline bool isMeta() const { return IsMeta; }

  bool addMeta(sv uri, sv name, sv title = {}, sv desc = {}, sv mime = {}, size_t size = 0);
  bool addText(std::string_view uri, std::string_view text, std::string_view mime = {});
  bool addBinary(std::string_view uri, std::span<uint8_t> data, std::string_view mime = {});

  nlohmann::json const&& popResult();

  private:
  nlohmann::json Result = nlohmann::json::array();
  bool           IsMeta = false;
};

class MCPContent: public MCPAnnotation {
  public:
  void setErrorFlag();

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
};

class MCPIO {
  private:
  using json  = nlohmann::json;
  using tcall = std::function<void(json const& req, MCPContent& resp)>;
  using rcall = std::function<void(json const& req, MCPResources& resp)>;

  struct Tool {
    json const  Info;
    tcall const Callback;
  };

  struct Resource {
    std::string const URI;
    rcall const       Callback;
  };

  bool Initialized = false;

  std::mutex Mutex;

  std::list<Tool>     Tools     = {};
  std::list<Resource> Resources = {};

  void sendResponse(std::optional<uint64_t> req_id, json const&& resp) const;
  void sendProtocolError(std::optional<uint64_t> req_id, int32_t code, std::string_view err, json const&& data = nullptr) const;
  void sendError(std::optional<uint64_t> req_id, std::string_view err) const;
  void sendNotification(std::string_view noti) const;

  auto findTool(std::string_view toolName) {
    auto it = Tools.begin();
    for (; it != Tools.end(); ++it) {
      if (it->Info["name"].get_ref<std::string const&>() == toolName) break;
    }

    return it;
  }

  auto findResource(std::string_view resURI) {
    auto it = Resources.begin();
    for (; it != Resources.end(); ++it) {
      if (it->URI == resURI) break;
    }

    return it;
  }

  bool makeStep(std::string_view input);

  public:
  bool registerTool(json const&& toolDesc, tcall callback);
  bool unregisterTool(std::string_view name);
  bool registerResource(std::string_view uri, rcall callback);
  bool unregisterResource(std::string_view uri);

  void startLoop();
};
