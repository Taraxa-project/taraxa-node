#include "config/config.hpp"

#include <json/json.h>

#include <fstream>

#include "common/jsoncpp.hpp"
#include "config/config_utils.hpp"

namespace taraxa {

void FullNodeConfig::validate() const {
  genesis.validate();
  network.validate(genesis.state.dpos.delegation_delay);
  if (transactions_pool_size < kMinTransactionPoolSize) {
    throw ConfigException("transactions_pool_size cannot be smaller than " + std::to_string(kMinTransactionPoolSize) +
                          ".");
  }

  if (is_light_node) {
    const auto min_light_node_history = (genesis.state.dpos.blocks_per_year * kDefaultLightNodeHistoryDays) / 365;
    if (light_node_history < min_light_node_history) {
      throw ConfigException("Min. required light node history is " + std::to_string(min_light_node_history) +
                            " blocks (" + std::to_string(kDefaultLightNodeHistoryDays) + " days)");
    }
  }

  // TODO: add validation of other config values
}

Json::Value FullNodeConfig::toJson() const {
  Json::Value json_config;

  json_config["data_path"] = data_path.string();
  json_config["is_light_node"] = is_light_node;
  json_config["light_node_history"] = light_node_history;
  json_config["dag_expiry_limit"] = dag_expiry_limit;
  json_config["max_levels_per_period"] = max_levels_per_period;
  json_config["enable_test_rpc"] = enable_test_rpc;
  json_config["final_chain_cache_in_blocks"] = final_chain_cache_in_blocks;
  json_config["transactions_pool_size"] = transactions_pool_size;

  json_config["db_config"] = db_config.toJson();
  json_config["network"] = network.toJson();

  auto& logging_json = json_config["logging"] = Json::Value(Json::objectValue);
  logging_json["log_path"] = log_path.string();

  auto& logging_configs_json = logging_json["configurations"] = Json::Value(Json::arrayValue);
  for (const auto& logging_config : log_configs) {
    logging_configs_json.append(logging_config.toJson());
  }

  return json_config;
}

Json::Value FullNodeConfig::walletToJson() const {
  Json::Value json_config;

  const auto key_pair = dev::KeyPair(node_secret);
  json_config["node_address"] = key_pair.address().toString();
  json_config["node_public"] = key_pair.pub().toString();
  json_config["node_secret"] = toHex(key_pair.secret().ref());

  json_config["vrf_public"] = taraxa::vrf_wrapper::getVrfPublicKey(vrf_secret).toString();
  json_config["vrf_secret"] = vrf_secret.toString();

  return json_config;
}

Json::Value DBConfig::toJson() const {
  Json::Value json_config;

  json_config["db_snapshot_each_n_pbft_block"] = db_snapshot_each_n_pbft_block;
  json_config["db_max_snapshots"] = db_max_snapshots;
  json_config["db_max_open_files"] = db_max_open_files;
  json_config["db_revert_to_period"] = db_revert_to_period;
  json_config["rebuild_db"] = rebuild_db;
  json_config["rebuild_db_period"] = rebuild_db_period;
  json_config["rebuild_db_columns"] = rebuild_db_columns;

  return json_config;
}

}  // namespace taraxa
