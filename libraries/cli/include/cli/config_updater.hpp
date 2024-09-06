#pragma once

#include <json/value.h>

#include <functional>
#include <vector>

namespace taraxa::cli {

class ConfigUpdater {
 public:
  ConfigUpdater(int chain_id);

  void UpdateConfig(Json::Value& old_conf);

 private:
  struct ConfigChange {
    using ApplyFunction = std::function<void(Json::Value& old_conf, const Json::Value& new_conf)>;

    ConfigChange() = default;
    ConfigChange(ApplyFunction&& apply_function) : apply(std::move(apply_function)) {}
    ApplyFunction apply;
  };

  Json::Value new_conf_;
  std::vector<ConfigChange> config_changes_;
};

}  // namespace taraxa::cli