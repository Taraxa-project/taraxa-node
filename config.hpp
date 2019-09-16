#ifndef TARAXA_NODE_CONFIG_HPP
#define TARAXA_NODE_CONFIG_HPP
#include <string>
#include "genesis_state.hpp"
#include "types.hpp"
#include "util.hpp"

// TODO make all the classes json/ptree (ser|de)serializable and use it in
// << operator
namespace taraxa {
struct RpcConfig {
  RpcConfig() = default;
  RpcConfig(std::string const &json_file);
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
  uint16_t network_transaction_interval;
  std::string network_id;
  bool network_encrypted;
  bool network_performance_log;
};

// Parameter Tuning purpose
struct TestParamsConfig {
  std::vector<uint> block_proposer;  // test_params.block_proposer
  std::vector<uint> pbft;            // test_params.pbft
};

struct FullNodeConfig {
  FullNodeConfig(std::string const &json_file);
  std::string json_file_name;
  std::string node_secret;
  std::string db_path;
  uint16_t dag_processing_threads;
  NetworkConfig network;
  RpcConfig rpc;
  TestParamsConfig test_params;
  GenesisState genesis_state;

  auto account_db_path() { return db_path + "/acc"; }
  auto account_snapshot_db_path() { return db_path + "/acc_snapshots"; }
  auto block_db_path() { return db_path + "/blk"; }
  auto block_index_db_path() { return db_path + "/blk_index"; }
  auto transactions_db_path() { return db_path + "/trx"; }
  auto pbft_votes_db_path() { return db_path + "/pbftvotes"; }
  auto pbft_chain_db_path() { return db_path + "/pbftchain"; }
  auto trxs_to_blk_db_path() { return db_path + "/trxs_to_blk"; }
  auto dag_blk_to_state_root_db_path() {
    return db_path + "/blk_to_state_root";
  }
};

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa
#endif
