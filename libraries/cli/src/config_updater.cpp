#include "cli/config_updater.hpp"

#include "cli/tools.hpp"
#include "config/version.hpp"

namespace taraxa::cli {

//// NetworkIPChange
bool NetworkIPChange::ShouldApply(uint32_t version) const {
  if (version < 1) return true;
  return false;
}

void NetworkIPChange::Apply(Json::Value& old_conf, const Json::Value& new_conf) const {
  if (!old_conf["network_address"].isNull()) {
    old_conf["network_listen_ip"] = old_conf["network_address"];
    old_conf.removeMember("network_address");
  } else if (old_conf["network_listen_ip"].isNull()) {
    old_conf["network_listen_ip"] = new_conf["network_listen_ip"];
  }
}
//// NetworkIPChange

ConfigUpdater::ConfigUpdater(int network_id) {
  new_conf_ = Tools::generateConfig((Config::NetworkIdType)network_id);
  // Regiser changes that should applay
  config_changes_.push_back(std::make_unique<NetworkIPChange>());
}

void ConfigUpdater::UpdateConfig(const std::string& config_name, Json::Value& old_conf) {
  uint32_t version = 0;
  if (!old_conf["version"].isNull()) {
    version = old_conf["version"].asUInt();
  }
  for (const auto& change : config_changes_) {
    if (change->ShouldApply(version)) {
      change->Apply(old_conf, new_conf_);
    }
  }
  old_conf["version"] = TARAXA_CONFIG_VERSION;
  Tools::writeJsonToFile(config_name, old_conf);
}

}  // namespace taraxa::cli