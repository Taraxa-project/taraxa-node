#pragma once

#include <string>

#include "common/config_exception.hpp"
#include "common/types.hpp"
#include "common/util.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/chain_config.hpp"
#include "logger/logger_config.hpp"

namespace taraxa {

struct RpcConfig {
  std::optional<uint16_t> http_port;
  std::optional<uint16_t> ws_port;
  boost::asio::ip::address address;

  // Number of threads dedicated to the rpc calls processing, default = 5
  uint16_t threads_num{5};

  void validate() const;
};

struct NodeConfig {
  std::string id;
  std::string ip;
  uint16_t udp_port = 0;
};

struct NetworkConfig {
  static constexpr uint16_t kBlacklistTimeoutDefaultInSeconds = 600;

  std::string json_file_name;
  std::string network_public_ip;
  std::string network_listen_ip;
  uint16_t network_tcp_port = 0;
  std::vector<NodeConfig> network_boot_nodes;
  uint16_t network_ideal_peer_count = 0;
  uint16_t network_max_peer_count = 0;
  uint16_t network_transaction_interval = 0;
  uint16_t network_sync_level_size = 0;
  uint64_t network_id;
  uint16_t network_performance_log_interval = 0;
  uint16_t network_num_threads = std::max(uint(1), uint(std::thread::hardware_concurrency() / 2));
  uint16_t network_packets_processing_threads = 14;
  uint16_t network_peer_blacklist_timeout = kBlacklistTimeoutDefaultInSeconds;
  bool disable_peer_blacklist = false;
  uint16_t deep_syncing_threshold = 10;

  void validate() const;
};

struct DBConfig {
  uint32_t db_snapshot_each_n_pbft_block = 0;
  uint32_t db_max_snapshots = 0;
  uint32_t db_max_open_files = 0;
  uint32_t db_revert_to_period = 0;
  bool rebuild_db = false;
  uint64_t rebuild_db_period = 0;
  bool rebuild_db_columns = false;
};

struct FullNodeConfig {
  static const uint32_t kDefaultLightNodeHistoryDays = 7;

  FullNodeConfig() = default;
  // The reason of using Json::Value as a union is that in the tests
  // there are attempts to pass char const* to this constructor, which
  // is ambiguous (char const* may promote to Json::Value)
  // if you have std::string and Json::Value constructor. It was easier
  // to just treat Json::Value as a std::string or Json::Value depending on
  // the contents
  explicit FullNodeConfig(Json::Value const &file_name_str_or_json_object, Json::Value const &wallet,
                          const std::string &config_file_path = "");
  std::string json_file_name;
  dev::Secret node_secret;
  vrf_wrapper::vrf_sk_t vrf_secret;
  fs::path data_path;
  fs::path db_path;
  fs::path log_path;
  NetworkConfig network;
  std::optional<RpcConfig> rpc;
  DBConfig db_config;
  ChainConfig chain = ChainConfig::predefined();
  state_api::Opts opts_final_chain;
  std::vector<logger::Config> log_configs;
  bool is_light_node = false;                            // Is light node
  uint64_t light_node_history = 0;                       // Number of periods to keep in history for a light node
  uint32_t dag_expiry_limit = kDagExpiryLevelLimit;      // For unit tests only
  uint32_t max_levels_per_period = kMaxLevelsPerPeriod;  // For unit tests only

  // config values that limits transactions and blocks memory pools
  uint32_t max_transactions_pool_warn = 0;
  uint32_t max_transactions_pool_drop = 0;

  auto net_file_path() const { return data_path / "net"; }

  /**
   * @brief Validates config values, throws configexception if validation failes
   * @return
   */
  void validate() const;

  void overwrite_chain_config_in_file() const;
};

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa
