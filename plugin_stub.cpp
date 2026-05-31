#include "plugin_api.hh"

class PluginMachine: public IPluginMachine {
  public:
  PluginMachine(std::filesystem::path const& pluginsDir) {}

  ~PluginMachine() {}

  bool handleArgument(std::string_view arg, std::stringstream& value) const final { return false; }

  void registerStuff(std::shared_ptr<IMCPIO> mcp) const final {}

  void printUsage(std::ostream& strm) const final {}
};

std::unique_ptr<IPluginMachine> createPluginMachine(std::filesystem::path const& pluginsDir) {
  return std::make_unique<PluginMachine>(pluginsDir);
}
