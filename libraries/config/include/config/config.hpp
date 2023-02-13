#pragma once

#include "common/config_exception.hpp"
#include "common/util.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/genesis.hpp"
#include "config/network.hpp"
#include "logger/logger_config.hpp"

namespace taraxa {

struct DBConfig {
  uint32_t db_snapshot_each_n_pbft_block = 0;
  uint32_t db_max_snapshots = 0;
  uint32_t db_max_open_files = 0;
  PbftPeriod db_revert_to_period = 0;
  bool rebuild_db = false;
  PbftPeriod rebuild_db_period = 0;
  bool rebuild_db_columns = false;

  /**
   * @brief Generate json from config object
   *
   * @return json representation of config
   */
  Json::Value toJson() const;
};

struct FullNodeConfig {
  static constexpr uint64_t kDefaultLightNodeHistoryDays = 7;

  FullNodeConfig() = default;

  std::string json_file_name;
  dev::Secret node_secret;
  vrf_wrapper::vrf_sk_t vrf_secret;
  fs::path data_path;
  fs::path db_path;
  fs::path log_path;
  NetworkConfig network;
  DBConfig db_config;
  Genesis genesis;
  state_api::Opts opts_final_chain;
  std::vector<logger::Config> log_configs;
  bool is_light_node = false;                            // Is light node
  uint64_t light_node_history = 0;                       // Number of periods to keep in history for a light node
  uint32_t dag_expiry_limit = kDagExpiryLevelLimit;      // For unit tests only
  uint32_t max_levels_per_period = kMaxLevelsPerPeriod;  // For unit tests only
  bool enable_test_rpc = false;
  bool enable_debug = false;
  uint32_t final_chain_cache_in_blocks = 5;

  // config values that limits transactions pool
  uint32_t transactions_pool_size = kDefaultTransactionPoolSize;

  auto net_file_path() const { return data_path / "net"; }

  /**
   * @brief Validates config values, throws configexception if validation failes
   * @return
   */
  void validate() const;

  /**
   * @brief Generate json from config object
   *
   * @return json representation of config
   */
  Json::Value toJson() const;

  /**
   * @brief Generate wallet json from config object
   *
   * @return json representation of wallet
   */
  Json::Value walletToJson() const;
};

}  // namespace taraxa
