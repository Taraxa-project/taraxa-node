#pragma once

#include <string>

#include "chain/chain_config.hpp"
#include "common/types.hpp"
#include "config/config_exception.hpp"
#include "dag/dag_block.hpp"
#include "logger/logger_config.hpp"
#include "util/util.hpp"

namespace taraxa {

struct RpcConfig {
  optional<uint16_t> http_port;
  optional<uint16_t> ws_port;
  boost::asio::ip::address address;

  // Number of threads dedicated to the rpc calls processing, default = 5
  uint16_t threads_num{5};
};

struct NodeConfig {
  std::string id;
  std::string ip;
  uint16_t tcp_port = 0;
  uint16_t udp_port = 0;
};

struct NetworkConfig {
  std::string json_file_name;
  bool network_is_boot_node = 0;
  std::string network_address;
  uint16_t network_tcp_port = 0;
  uint16_t network_udp_port = 0;
  std::vector<NodeConfig> network_boot_nodes;
  uint16_t network_simulated_delay = 0;
  uint16_t network_bandwidth = 0;
  uint16_t network_ideal_peer_count = 0;
  uint16_t network_max_peer_count = 0;
  uint16_t network_transaction_interval = 0;
  uint16_t network_min_dag_block_broadcast = 0;
  uint16_t network_max_dag_block_broadcast = 0;
  uint16_t network_sync_level_size = 0;
  uint64_t network_id;
  bool network_encrypted = 0;
  bool network_performance_log = 0;
  bool net_log = 0;
};

struct BlockProposerConfig {
  uint16_t shard = 0;
  uint16_t transaction_limit = 0;
};

// Parameter Tuning purpose
struct TestParamsConfig {
  BlockProposerConfig block_proposer;  // test_params.block_proposer
  uint32_t max_transaction_queue_warn = 0;
  uint32_t max_transaction_queue_drop = 0;
  uint32_t max_block_queue_warn = 0;
  uint32_t db_snapshot_each_n_pbft_block = 0;
  uint32_t db_max_snapshots = 0;
  uint32_t db_revert_to_period = 0;
  bool rebuild_db = 0;
  uint64_t rebuild_db_period = 0;
};

struct FullNodeConfig {
  FullNodeConfig() = default;
  // The reason of using Json::Value as a union is that in the tests
  // there are attempts to pass char const* to this constructor, which
  // is ambiguous (char const* may promote to Json::Value)
  // if you have std::string and Json::Value constructor. It was easier
  // to just treat Json::Value as a std::string or Json::Value depending on
  // the contents
  explicit FullNodeConfig(Json::Value const &file_name_str_or_json_object,
                          Json::Value const &chain_file_name_str_or_json_object = "");
  std::string json_file_name;
  std::string node_secret;
  vrf_wrapper::vrf_sk_t vrf_secret;
  fs::path db_path;
  NetworkConfig network;
  optional<RpcConfig> rpc;
  TestParamsConfig test_params;
  ChainConfig chain = ChainConfig::predefined();
  FinalChain::Opts opts_final_chain;
  std::vector<logger::Config> log_configs;

  auto net_file_path() const { return db_path / "net"; }

  /**
   * @brief Validates config values
   * @return true in case config is valid, otherwise false
   */
  bool validate();
};

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa
