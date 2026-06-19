#pragma once

#include "../../third_party/json.hpp"

#include <memory>

class ISimpleMem {
  public:
  ISimpleMem()          = default;
  virtual ~ISimpleMem() = default;

  virtual nlohmann::json getList() const                         = 0;
  virtual nlohmann::json addMem(nlohmann::json const& req)       = 0;
  virtual nlohmann::json delMem(nlohmann::json const& req)       = 0;
  virtual nlohmann::json getMem(nlohmann::json const& req) const = 0;
};

std::unique_ptr<ISimpleMem> createMemory();
