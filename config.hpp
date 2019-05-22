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

struct ProposerConfig {
  uint mode;
  uint param1;
  uint param2;
  uint shards;
};

struct NetworkConfig {
  NetworkConfig() = default;
  std::string json_file_name;
  std::string network_address;
  uint16_t network_listen_port;
  std::vector<NodeConfig> network_boot_nodes;
  uint16_t network_simulated_delay;
  uint16_t network_bandwidth;
};

struct PbftManagerConfig {
  u_long lambda_ms;
};

struct FullNodeConfig {
  FullNodeConfig(std::string const &json_file);
  std::string json_file_name;
  std::string node_secret;
  std::string db_accounts_path;
  std::string db_blocks_path;
  std::string db_transactions_path;
  uint16_t dag_processing_threads;
  ProposerConfig proposer;
  NetworkConfig network;
  RpcConfig rpc;
  PbftManagerConfig pbft_manager;
  std::string db_pbft_votes_path;
  std::string db_pbft_chain_path;
};
std::ostream &operator<<(std::ostream &strm, RpcConfig const &conf);
std::ostream &operator<<(std::ostream &strm, PbftManagerConfig const &conf);
std::ostream &operator<<(std::ostream &strm, ProposerConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa
#endif
