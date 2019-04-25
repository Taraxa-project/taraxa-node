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
#include <boost/filesystem.hpp>

namespace taraxa {
struct RpcConfig {
  RpcConfig()=default;
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
};

struct NetworkConfig {
  NetworkConfig() = default;
  std::string json_file_name;
  std::string network_address;
  uint16_t network_listen_port;
  std::vector<NodeConfig> network_boot_nodes;
  uint16_t network_simulated_delay;
};

struct FullNodeConfig {
  FullNodeConfig(std::string const &json_file);
  std::string json_file_name;
  std::string node_secret;
  boost::filesystem::path node_db_path;
  boost::filesystem::path node_state_path;
  uint16_t dag_processing_threads;
  ProposerConfig proposer;
  NetworkConfig network;
  RpcConfig rpc;
};
}  // namespace taraxa
#endif
