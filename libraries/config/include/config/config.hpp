#pragma once

#include <string>

#include "common/config_exception.hpp"
#include "common/types.hpp"
#include "common/util.hpp"
#include "common/vrf_wrapper.hpp"
#include "config/genesis.hpp"
#include "logger/logger_config.hpp"

namespace taraxa {

struct ConnectionConfig {
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
  uint16_t port = 0;
};

struct NetworkConfig {
  static constexpr uint16_t kBlacklistTimeoutDefaultInSeconds = 600;

  std::string json_file_name;
  std::string public_ip;
  std::string listen_ip;
  uint16_t listen_port = 0;
  std::vector<NodeConfig> boot_nodes;
  uint16_t ideal_peer_count = 0;
  uint16_t max_peer_count = 0;
  uint16_t transaction_interval_ms = 0;
  uint16_t sync_level_size = 0;
  uint16_t performance_log_interval = 0;
  uint16_t num_threads = std::max(uint(1), uint(std::thread::hardware_concurrency() / 2));
  uint16_t packets_processing_threads = 14;
  uint16_t peer_blacklist_timeout = kBlacklistTimeoutDefaultInSeconds;
  bool disable_peer_blacklist = false;
  uint16_t deep_syncing_threshold = 10;
  PbftPeriod vote_accepting_periods = 5;
  PbftRound vote_accepting_rounds = 5;
  PbftStep vote_accepting_steps = 0;

  void validate() const;
};

struct DBConfig {
  uint32_t db_snapshot_each_n_pbft_block = 0;
  uint32_t db_max_snapshots = 0;
  uint32_t db_max_open_files = 0;
  PbftPeriod db_revert_to_period = 0;
  bool rebuild_db = false;
  PbftPeriod rebuild_db_period = 0;
  bool rebuild_db_columns = false;
};

struct FullNodeConfig {
  static constexpr uint64_t kDefaultLightNodeHistoryDays = 7;

  FullNodeConfig() = default;
  // The reason of using Json::Value as a union is that in the tests
  // there are attempts to pass char const* to this constructor, which
  // is ambiguous (char const* may promote to Json::Value)
  // if you have std::string and Json::Value constructor. It was easier
  // to just treat Json::Value as a std::string or Json::Value depending on
  // the contents
  explicit FullNodeConfig(Json::Value const &file_name_str_or_json_object, Json::Value const &wallet,
                          Json::Value const &genesis = Json::Value::null, const std::string &config_file_path = "");

  explicit FullNodeConfig(const std::string &chain_config_name) : genesis(Genesis::predefined(chain_config_name)) {}
  std::string json_file_name;
  dev::Secret node_secret;
  vrf_wrapper::vrf_sk_t vrf_secret;
  fs::path data_path;
  fs::path db_path;
  fs::path log_path;
  NetworkConfig network;
  std::optional<ConnectionConfig> rpc;
  std::optional<ConnectionConfig> graphql;
  DBConfig db_config;
  Genesis genesis = Genesis::predefined();
  uint64_t chain_id = 0;
  state_api::Opts opts_final_chain;
  std::vector<logger::Config> log_configs;
  bool is_light_node = false;                            // Is light node
  uint64_t light_node_history = 0;                       // Number of periods to keep in history for a light node
  uint32_t dag_expiry_limit = kDagExpiryLevelLimit;      // For unit tests only
  uint32_t max_levels_per_period = kMaxLevelsPerPeriod;  // For unit tests only
  bool enable_test_rpc = false;
  uint32_t final_chain_cache_in_blocks = 5;

  // config values that limits transactions pool
  uint32_t transactions_pool_size = kDefaultTransactionPoolSize;

  auto net_file_path() const { return data_path / "net"; }

  /**
   * @brief Validates config values, throws configexception if validation failes
   * @return
   */
  void validate() const;
};

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa
