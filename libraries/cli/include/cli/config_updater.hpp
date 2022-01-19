#pragma once

#include <string>

#include "config/config.hpp"

namespace taraxa::cli {

class ConfigChange {
 public:
  ConfigChange() = default;
  virtual ~ConfigChange() = default;

  virtual bool ShouldApply(uint32_t version) const = 0;
  virtual void Apply(Json::Value& old_conf, const Json::Value& new_conf) const = 0;
};

class NetworkIPChange : public ConfigChange {
 public:
  NetworkIPChange() = default;
  virtual ~NetworkIPChange() = default;

  bool ShouldApply(uint32_t version) const;
  void Apply(Json::Value& old_conf, const Json::Value& new_conf) const;
};

class ConfigUpdater {
 public:
  ConfigUpdater(int network_id);
  ~ConfigUpdater() = default;

  void UpdateConfig(const std::string& config_name, Json::Value& old_conf);

 private:
  Json::Value new_conf_;
  std::vector<std::unique_ptr<ConfigChange>> config_changes_;
};

}  // namespace taraxa::cli