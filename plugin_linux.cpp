#include "plugin_api.hh"

#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <map>

class PluginMachine: public IPluginMachine {
  public:
  PluginMachine(std::filesystem::path const& pluginsDir) {
    if (std::filesystem::is_directory(pluginsDir)) {
      for (auto& ent: std::filesystem::directory_iterator(pluginsDir)) {
        if (!ent.is_regular_file()) continue;
        std::error_code ec;

        auto const perms = ent.status(ec).permissions();
        if (ec) {
          std::cerr << "Skipping file " << ent.path() << ": failed to get fs::status" << std::endl;
          continue;
        }
        constexpr auto exec_mask = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
        if ((perms & exec_mask) == std::filesystem::perms::none) {
          std::cerr << "Skipping file " << ent.path() << ": not a excecutable file" << std::endl;
          continue;
        }
        auto plugin = dlopen(ent.path().string().c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (plugin == nullptr) {
          std::cerr << "Skipping file " << ent.path() << ": failed to dlopen()" << std::endl;
          continue;
        }

        Plugins[ent.path().filename().string()] = plugin;
      }
    }
  }

  ~PluginMachine() {
    for (auto it = Plugins.begin(); it != Plugins.end();) {
      dlclose(it->second);
      it = Plugins.erase(it);
    }
  }

  bool handleArgument(std::string_view arg, std::stringstream& value) const final {
    for (auto const& plugin: Plugins) {
      auto argHandler = reinterpret_cast<PluginArgHandler*>(dlsym(plugin.second, "LMCP_HandleArgument"));
      if (argHandler != nullptr && argHandler(arg, value)) return true;
    }

    return false;
  }

  void registerStuff(std::shared_ptr<IMCPIO> mcp) const final {
    for (auto const& plugin: Plugins) {
      auto regHandler = reinterpret_cast<PluginRegister*>(dlsym(plugin.second, "LMCP_RegisterStuff"));
      if (regHandler != nullptr) regHandler(mcp);
    }
  }

  void printUsage(std::ostream& strm) const final {
    for (auto const& plugin: Plugins) {
      auto printHandler = reinterpret_cast<PluginPrintUsage*>(dlsym(plugin.second, "LMCP_PrintUsage"));
      if (printHandler != nullptr) printHandler(strm);
    }
  }

  private:
  std::map<std::string, void*> Plugins;
};

std::unique_ptr<IPluginMachine> createPluginMachine(std::filesystem::path const& pluginsDir) {
  return std::make_unique<PluginMachine>(pluginsDir);
}
