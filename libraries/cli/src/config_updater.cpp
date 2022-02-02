#include "cli/config_updater.hpp"

#include "cli/tools.hpp"
#include "config/version.hpp"

namespace taraxa::cli {

//// NetworkIPChange
auto NetworkIPChange = [](Json::Value& old_conf, const Json::Value& new_conf) {
  if (!old_conf["network_address"].isNull()) {
    old_conf["network_listen_ip"] = old_conf["network_address"];
    old_conf.removeMember("network_address");
  } else if (old_conf["network_listen_ip"].isNull()) {
    old_conf["network_listen_ip"] = new_conf["network_listen_ip"];
  }
};
//// NetworkIPChange

ConfigUpdater::ConfigUpdater(int network_id) {
  new_conf_ = Tools::generateConfig(static_cast<Config::NetworkIdType>(network_id));
  // Regiser changes that should apply
  config_changes_.emplace_back(NetworkIPChange);
}

void ConfigUpdater::UpdateConfig(const std::string& config_name, Json::Value& old_conf) {
  for (const auto& change : config_changes_) {
    change.apply(old_conf, new_conf_);
  }
  Tools::writeJsonToFile(config_name, old_conf);
}

}  // namespace taraxa::cli