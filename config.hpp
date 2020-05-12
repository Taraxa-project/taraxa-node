#ifndef TARAXA_NODE_CONFIG_HPP
#define TARAXA_NODE_CONFIG_HPP

#include <string>

#include "conf/chain_config.hpp"
#include "dag_block.hpp"
#include "types.hpp"
#include "util.hpp"

// TODO: Generate configs for the tests
// TODO: Separate configs for consensus chain params and technical params
// TODO: Expose only certain eth chain params, encapsulate the config invariants
namespace taraxa {

class ConfigException : public std::exception {
 public:
  ConfigException(std::string msg) : msg(msg) {}
  const char *what() const throw() { return msg.c_str(); }

 private:
  std::string msg;
};

struct RpcConfig {
  RpcConfig() = default;
  explicit RpcConfig(std::string const &json_file);
  std::string json_file_name;
  uint16_t port;
  uint16_t ws_port;
  boost::asio::ip::address address;
};

struct NodeConfig {
  std::string id;
  std::string ip;
  uint16_t port;
};

struct NetworkConfig {
  NetworkConfig() = default;
  std::string json_file_name;
  std::string network_address;
  uint16_t network_listen_port;
  std::vector<NodeConfig> network_boot_nodes;
  uint16_t network_simulated_delay;
  uint16_t network_bandwidth;
  uint16_t network_ideal_peer_count;
  uint16_t network_max_peer_count;
  uint16_t network_transaction_interval;
  uint16_t network_sync_level_size;
  std::string network_id;
  bool network_encrypted;
  bool network_performance_log;
};

struct BlockProposerConfig {
  std::string mode;
  uint16_t shard;
  uint16_t transaction_limit;
  // Random mode params
  uint16_t min_freq;
  uint16_t max_freq;
  // Sortition mode params
  uint16_t difficulty_bound;
  uint16_t lambda_bits;
};

struct PbftConfig {
  uint32_t lambda_ms_min;
  uint32_t committee_size = 0;
  uint64_t valid_sortition_coins = 0;
  uint32_t dag_blocks_size = 0;
  uint32_t ghost_path_move_back = 0;
  uint64_t skip_periods = 0;
  bool run_count_votes = false;
};

// Parameter Tuning purpose
struct TestParamsConfig {
  BlockProposerConfig block_proposer;  // test_params.block_proposer
  PbftConfig pbft;                     // test_params.pbft
  uint32_t max_transaction_queue_warn = 0;
  uint32_t max_transaction_queue_drop = 0;
  uint32_t max_block_queue_warn = 0;
};

struct FullNodeConfig {
  explicit FullNodeConfig(std::string const &json_file);
  std::string json_file_name;
  std::string node_secret;
  std::string vrf_secret;
  std::string db_path;
  uint16_t dag_processing_threads;
  NetworkConfig network;
  RpcConfig rpc;
  TestParamsConfig test_params;
  conf::chain_config::ChainConfig chain;
};

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa
#endif
