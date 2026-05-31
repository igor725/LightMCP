#pragma once

#include "../../third_party/json.hpp"

#include <filesystem>
#include <memory>

class ISearch {
  public:
  ISearch()          = default;
  virtual ~ISearch() = default;

  virtual nlohmann::json getList() const                              = 0;
  virtual nlohmann::json searchFuzzy(nlohmann::json const& req) const = 0;
  virtual nlohmann::json openFile(nlohmann::json const& req) const    = 0;
};

std::unique_ptr<ISearch> createSearch(std::filesystem::path const& path);
