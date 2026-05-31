#pragma once

#include "mcp_io.hh" // IWYU pragma: export

#include <filesystem>
#include <memory>

typedef bool(PluginArgHandler)(std::string_view arg, std::stringstream& value);
typedef void(PluginRegister)(std::shared_ptr<IMCPIO> mcp);
typedef void(PluginPrintUsage)(std::ostream& strm);

class IPluginMachine {
  public:
  IPluginMachine()          = default;
  virtual ~IPluginMachine() = default;

  virtual bool handleArgument(std::string_view arg, std::stringstream& value) const = 0;

  virtual void registerStuff(std::shared_ptr<IMCPIO> mcp) const = 0;

  virtual void printUsage(std::ostream& strm) const = 0;
};

std::unique_ptr<IPluginMachine> createPluginMachine(std::filesystem::path const& pluginsDir);
