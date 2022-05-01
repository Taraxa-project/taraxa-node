#pragma once

#include <string>

#include "config/config.hpp"

namespace taraxa::cli {

class ConfigUpdater {
 public:
  ConfigUpdater(int network_id);
  ~ConfigUpdater() = default;

  void UpdateConfig(const std::string& config_name, Json::Value& old_conf);

 private:
  struct ConfigChange {
    ConfigChange() = default;
    ConfigChange(auto&& apply_function) : apply(apply_function) {}
    std::function<void(Json::Value& old_conf, const Json::Value& new_conf)> apply;
  };

  Json::Value new_conf_;
  std::vector<ConfigChange> config_changes_;
};

}  // namespace taraxa::cli