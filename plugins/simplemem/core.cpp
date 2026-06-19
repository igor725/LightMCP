#include "core.hh"

class SimpleMem: public ISimpleMem {
  nlohmann::json Memories = nlohmann::json::object();

  template <typename Self>
  auto findMem(this Self&& self, std::string const& name) {
    for (auto it = self.Memories.begin(); it != self.Memories.end(); ++it) {
      if (it.key() == name) return it;
    }

    return self.Memories.end();
  }

  public:
  SimpleMem()          = default;
  virtual ~SimpleMem() = default;

  nlohmann::json getList() const {

    nlohmann::json list = nlohmann::json::array();

    for (auto const& [key, value]: Memories.items()) {
      list.emplace_back(key);
    }

    return nlohmann::json::object({{"list", std::move(list)}});
  }

  nlohmann::json addMem(nlohmann::json const& req) {
    if (findMem(req["name"].get_ref<std::string const&>()) == Memories.end()) {
      Memories[req["name"]] = req["contents"];
      return {{"result", true}};
    }

    return {{"result", false}};
  }

  nlohmann::json delMem(nlohmann::json const& req) {
    if (auto it = findMem(req["name"].get_ref<std::string const&>()); it != Memories.end()) {
      Memories.erase(it);
      return {{"found", true}};
    }

    return {{"found", false}};
  }

  nlohmann::json getMem(nlohmann::json const& req) const {
    if (auto it = findMem(req["name"].get_ref<std::string const&>()); it != Memories.end()) {
      return {
          {"found", true},
          {"result", it.value()},
      };
    }

    return {{"found", false}};
  }
};

std::unique_ptr<ISimpleMem> createMemory() {
  return std::make_unique<SimpleMem>();
}
