#pragma once

#include "third_party/json.hpp"

#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <stack>

class MCPAnnotation {
  public:
  using Clock = std::chrono::system_clock;

  nlohmann::json createAnnotation(bool userAttention, bool assistantAttention, float priority, Clock::time_point lastMod = {}) const;
};

class MCPAnnotations: public MCPAnnotation {

  void pushAnnotations(nlohmann::json const&& annotation);
  void popAnnotations();

  protected:
  std::stack<nlohmann::json> Annotations;
};

struct MCPResource: public MCPAnnotation {
  using cbfunc = std::function<nlohmann::json(bool metaOnly, nlohmann::json const& req, MCPResource const& self)>;

  std::string const URI;
  std::string const FileName;
  std::string const Title;
  std::string const Description;
  std::string const MimeType;
  cbfunc const      Callback;

  MCPResource(std::string_view uri, std::string_view name, std::string_view title, std::string_view desc, std::string_view mime, MCPResource::cbfunc callback)
      : URI(uri), FileName(name), Title(title), Description(desc), MimeType(mime), Callback(callback) {}

  nlohmann::json generateText(bool metaOnly, std::string_view string, nlohmann::json const&& annotation = {}) const;
  nlohmann::json generateBlob(bool metaOnly, std::span<uint8_t> data, nlohmann::json const&& annotation = {}) const;
};

class MCPResources {
  friend class MCPIO;
  using sv = std::string_view;

  public:
  MCPResources() {}

  bool add(nlohmann::json&& resource);

  private:
  nlohmann::json Result = nlohmann::json::array();

  protected:
  nlohmann::json popResult();
};

class MCPContent: public MCPAnnotations {
  public:
  void setErrorFlag();

  bool addText(std::string_view text);
  bool addImage(std::string_view mime, std::span<uint8_t> data);
  bool addAudio(std::string_view mime, std::span<uint8_t> data);
  bool addStructured(nlohmann::json const&& block);

  nlohmann::json popResult();

  private:
  nlohmann::json Result = {
      {"isError", false},
      {"content", nlohmann::json::array()},
  };
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

  std::list<Tool>        Tools     = {};
  std::list<MCPResource> Resources = {};

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
  bool registerResource(std::string_view uri, MCPResource::cbfunc callback, std::string_view name = {}, std::string_view title = {}, std::string_view desc = {},
                        std::string_view mime = {});
  bool unregisterResource(std::string_view uri);

  void startLoop();
};
