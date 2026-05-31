#pragma once

#include "third_party/json.hpp"

#include <chrono>
#include <cstdint>
#include <stack>

class MCPAnnotation {
  public:
  using Clock = std::chrono::system_clock;

  virtual nlohmann::json createAnnotation(bool userAttention, bool assistantAttention, float priority, Clock::time_point lastMod = {}) const;
};

class MCPAnnotations: public MCPAnnotation {
  virtual void pushAnnotations(nlohmann::json const&& annotation);
  virtual void popAnnotations();

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

  virtual nlohmann::json generateText(bool metaOnly, std::string_view string, nlohmann::json const&& annotation = {}) const;
  virtual nlohmann::json generateBlob(bool metaOnly, std::span<uint8_t> data, nlohmann::json const&& annotation = {}) const;
};

class MCPResources {
  friend class MCPIO;
  using sv = std::string_view;

  public:
  MCPResources() {}

  virtual bool add(nlohmann::json&& resource);

  private:
  nlohmann::json Result = nlohmann::json::array();

  protected:
  nlohmann::json popResult();
};

class MCPContent: public MCPAnnotations {
  public:
  virtual void setErrorFlag();

  virtual bool addText(std::string_view text);
  virtual bool addImage(std::string_view mime, std::span<uint8_t> data);
  virtual bool addAudio(std::string_view mime, std::span<uint8_t> data);
  virtual bool addStructured(nlohmann::json const&& block);

  virtual nlohmann::json popResult();

  private:
  nlohmann::json Result = {
      {"isError", false},
      {"content", nlohmann::json::array()},
  };
};

class IMCPIO {
  protected:
  using json  = nlohmann::json;
  using tcall = std::function<void(json const& req, MCPContent& resp)>;

  public:
  virtual bool registerTool(json const&& toolDesc, tcall callback)                      = 0;
  virtual bool unregisterTool(std::string_view name)                                    = 0;
  virtual bool registerResource(std::string_view uri, MCPResource::cbfunc callback, std::string_view name = {}, std::string_view title = {},
                                std::string_view desc = {}, std::string_view mime = {}) = 0;
  virtual bool unregisterResource(std::string_view uri)                                 = 0;

  virtual void startLoop() = 0;
};

std::shared_ptr<IMCPIO> createMCPServer();
