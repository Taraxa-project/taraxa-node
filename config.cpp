/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-04-23 16:26:39
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 17:29:14
 */

#include "config.hpp"
namespace taraxa {
FullNodeConfig::FullNodeConfig(std::string const &json_file)
    : json_file_name(json_file) {
  try {
    boost::property_tree::ptree doc = loadJsonFile(json_file);
    node_secret = doc.get<std::string>("node_secret");
    db_path = doc.get<std::string>("db_path");
    dag_processing_threads = doc.get<uint16_t>("dag_processing_threads");

    proposer.mode = doc.get<uint>("block_proposer.mode");
    proposer.param1 = doc.get<uint>("block_proposer.param1");
    proposer.param2 = doc.get<uint>("block_proposer.param2");
    proposer.shards = doc.get<uint>("block_proposer.shards");

    network.network_address = doc.get<std::string>("network_address");
    network.network_id = doc.get<std::string>("network_id");
    network.network_listen_port = doc.get<uint16_t>("network_listen_port");
    network.network_simulated_delay =
        doc.get<uint16_t>("network_simulated_delay");
    network.network_transaction_interval =
        doc.get<uint16_t>("network_transaction_interval");
    network.network_bandwidth = doc.get<uint16_t>("network_bandwidth");
    for (auto &item : doc.get_child("network_boot_nodes")) {
      NodeConfig node;
      node.id = item.second.get<std::string>("id");
      node.ip = item.second.get<std::string>("ip");
      node.port = item.second.get<uint16_t>("port");
      network.network_boot_nodes.push_back(node);
    }
    rpc.address =
        boost::asio::ip::address::from_string(network.network_address);
    rpc.port = doc.get<uint16_t>("rpc_port");
    pbft_manager.lambda_ms = doc.get<u_long>("pbft_manager.lambda_ms");

    {  // for test experiments
      for (auto &i : asVector<uint>(doc, "test_params.block_proposer")) {
        test_params.block_proposer.push_back(i);
      }

      for (auto &i : asVector<uint>(doc, "test_params.pbft")) {
        test_params.pbft.push_back(i);
      }
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

RpcConfig::RpcConfig(std::string const &json_file) : json_file_name(json_file) {
  try {
    boost::property_tree::ptree doc = loadJsonFile(json_file);
    port = doc.get<uint16_t>("port");
    address =
        boost::asio::ip::address::from_string(doc.get<std::string>("address"));
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

std::ostream &operator<<(std::ostream &strm, TestParamsConfig const &conf) {
  strm << "[TestNet Config] " << std::endl;
  strm << "   block_proposer: " << conf.block_proposer << std::endl;
  strm << "   pbft: " << conf.pbft << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, RpcConfig const &conf) {
  strm << "[Rpc Config] " << std::endl;
  strm << "   json_file_name: " << conf.json_file_name << std::endl;
  strm << "   port: " << conf.json_file_name << std::endl;
  strm << "   address: " << conf.address << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, PbftManagerConfig const &conf) {
  strm << "[PbftManager Config] " << std::endl;
  strm << "   lambda: " << conf.lambda_ms << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, ProposerConfig const &conf) {
  strm << "[Proposer Config] " << std::endl;
  strm << "  mode: " << conf.mode << std::endl;
  strm << "  param1: " << conf.param1 << std::endl;
  strm << "  param2: " << conf.param2 << std::endl;
  strm << "  shards: " << conf.shards << std::endl;
  return strm;
}

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf) {
  strm << "  [Node Config] " << std::endl;
  strm << "    node_id: " << conf.id << std::endl;
  strm << "    node_ip: " << conf.ip << std::endl;
  strm << "    node_port: " << conf.port << std::endl;
  return strm;
}
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf) {
  strm << "[Network Config] " << std::endl;
  strm << "  json_file_name: " << conf.json_file_name << std::endl;
  strm << "  network_address: " << conf.network_address << std::endl;
  strm << "  network_listen_port: " << conf.network_listen_port << std::endl;
  strm << "  network_simulated_delay: " << conf.network_simulated_delay
       << std::endl;
  strm << "  network_transaction_interval: "
       << conf.network_transaction_interval << std::endl;
  strm << "  network_bandwidth: " << conf.network_bandwidth << std::endl;
  strm << "  network_id: " << conf.network_id << std::endl;

  strm << "  --> boot nodes  ... " << std::endl;
  for (auto const &c : conf.network_boot_nodes) {
    strm << c << std::endl;
  }
  return strm;
}
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf) {
  strm << "[FullNode Config] " << std::endl;
  strm << "  json_file_name: " << conf.json_file_name << std::endl;
  strm << "  node_secret: " << conf.node_secret << std::endl;
  strm << "  db_path: " << conf.db_path << std::endl;
  strm << "  dag_processing_thread: " << conf.dag_processing_threads
       << std::endl;
  strm << conf.proposer;
  strm << conf.network;
  strm << conf.rpc;
  strm << conf.pbft_manager;
  strm << conf.test_params;
  return strm;
}
}  // namespace taraxa