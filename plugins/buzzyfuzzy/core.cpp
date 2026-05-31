#include "core.hh"

#include <fstream>
#include <map>
#include <rapidfuzz/fuzz.hpp>

class Search: public ISearch {
  public:
  Search(std::filesystem::path const& path): Root(path) {
    for (auto const& ent: std::filesystem::directory_iterator(path)) {
      if (!ent.is_directory()) continue;
      auto const indexFile = ent.path() / "core/file_index.json";
      auto const dataDir   = ent.path() / "data";
      if (!std::filesystem::is_regular_file(indexFile)) continue;
      if (!std::filesystem::is_directory(dataDir)) continue;
      std::map<std::string, std::string> files;

      {
        std::ifstream f(indexFile);

        for (auto const& mdFile: nlohmann::json::parse(f)) {
          files[mdFile["filename"].get_ref<std::string const&>()] = mdFile["title"].get_ref<std::string const&>();
        }
      }

      if (!files.empty()) List.emplace(ent.path().filename().string(), std::move(files));
    }
  }

  ~Search() {}

  nlohmann::json getList() const final {
    nlohmann::json result = nlohmann::json::array();

    for (auto it = List.begin(); it != List.end(); ++it) {
      result.push_back(it->first);
    }

    return {{"list", std::move(result)}};
  }

  nlohmann::json searchFuzzy(nlohmann::json const& req) const {
    auto prjName = req.find("project");
    auto query   = req.find("query");

    nlohmann::json result = {{"results", nlohmann::json::array()}};

    if (prjName == req.end() || query == req.end()) {
      result["error"] = "One of required parameters is missing";
      return std::move(result);
    }

    if (!prjName->is_string() || !query->is_string()) {
      result["error"] = "Both parameters should be strings";
      return std::move(result);
    }

    auto prjFiles = List.find(prjName->get_ref<std::string const&>());
    if (prjFiles == List.end()) {
      result["error"] = "Specified project not found";
      return std::move(result);
    }

    auto& resArray = result["results"];
    auto& queryStr = query->get_ref<std::string const&>();
    for (auto const& [file, title]: prjFiles->second) {
      auto const score = (rapidfuzz::fuzz::ratio(title, queryStr) + rapidfuzz::fuzz::ratio(file, queryStr)) / 2.0;
      if (score > 40.0) {
        resArray.emplace_back(nlohmann::json::object({
            {"title", title},
            {"filename", file},
            {"score", score},
        }));
      }
    }

    return std::move(result);
  }

  nlohmann::json openFile(nlohmann::json const& req) const {
    auto prjName  = req.find("project");
    auto fileName = req.find("filename");

    nlohmann::json result = {{"contents", nullptr}};

    if (prjName == req.end() || fileName == req.end()) {
      result["error"] = "One of required parameters is missing";
      return std::move(result);
    }

    if (!prjName->is_string() || !fileName->is_string()) {
      result["error"] = "Both parameters should be strings";
      return std::move(result);
    }

    auto& prjNameStr = prjName->get_ref<std::string const&>();

    auto prjFiles = List.find(prjNameStr);
    if (prjFiles == List.end()) {
      result["error"] = "Specified project not found";
      return std::move(result);
    }

    auto  resArray    = result["results"];
    auto& fileNameStr = fileName->get_ref<std::string const&>();

    if (auto file = prjFiles->second.find(fileNameStr); file != prjFiles->second.end()) {
      auto const dataDir  = Root / prjNameStr / "data";
      auto const filePath = (dataDir / fileNameStr).lexically_normal();
      if (!filePath.native().starts_with(dataDir.native())) {
        result["error"] = "Can't escape project directory";
        return std::move(result);
      }

      std::ifstream mdFile(filePath);
      if (mdFile.is_open()) {
        std::string contents((std::istreambuf_iterator<char>(mdFile)), std::istreambuf_iterator<char>());
        result["contents"] = std::move(contents);
      } else {
        result["error"] = "File not found on disk";
      }
    } else {
      result["error"] = "File not found in index";
    }

    return std::move(result);
  }

  private:
  std::filesystem::path const                               Root;
  std::map<std::string, std::map<std::string, std::string>> List;
};

std::unique_ptr<ISearch> createSearch(std::filesystem::path const& path) {
  return std::make_unique<Search>(path);
}
