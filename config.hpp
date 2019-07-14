/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-23 15:42:37
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 16:40:21
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <string>
#include "types.hpp"
#include "util.hpp"

namespace taraxa {
struct RpcConfig {
  RpcConfig() = default;
  RpcConfig(std::string const &json_file);
  std::string json_file_name;
  uint16_t port;
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

  auto account_db_path() { return db_path + "/acc"; }
  auto block_db_path() { return db_path + "/blk"; }
  auto block_index_db_path() { return db_path + "/blk_index"; }
  auto transactions_db_path() { return db_path + "/trx"; }
  auto pbft_votes_db_path() { return db_path + "/pbftvotes"; }
  auto pbft_chain_db_path() { return db_path + "/pbftchain"; }
  auto trxs_to_blk_db_path() { return db_path + "/trxs_to_blk"; }
};

std::ostream &operator<<(std::ostream &strm, TestParamsConfig const &conf);
std::ostream &operator<<(std::ostream &strm, RpcConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa
#endif
