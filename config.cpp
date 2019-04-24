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
    db_accounts_path = doc.get<std::string>("db_accounts_path");
    db_blocks_path = doc.get<std::string>("db_blocks_path");
    db_transactions_path = doc.get<std::string>("db_transactions_path");
    dag_processing_threads = doc.get<uint16_t>("dag_processing_threads");

    proposer.mode = doc.get<uint>("block_proposer.mode");
    proposer.param1 = doc.get<uint>("block_proposer.param1");
    proposer.param2 = doc.get<uint>("block_proposer.param2");
    network.network_address = doc.get<std::string>("network_address");
    network.network_listen_port = doc.get<uint16_t>("network_listen_port");
    network.network_simulated_delay = doc.get<uint16_t>("network_simulated_delay");
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
}  // namespace taraxa