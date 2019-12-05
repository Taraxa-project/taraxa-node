#include "config.hpp"
#include <fstream>
#include <libdevcore/LevelDB.h>

namespace taraxa {
FullNodeConfig::FullNodeConfig(std::string const &json_file)
    : json_file_name(json_file) {
  try {
    boost::property_tree::ptree doc = loadJsonFile(json_file);
    node_secret = doc.get<std::string>("node_secret");
    vrf_secret = doc.get<std::string>("vrf_secret");
    db_path = doc.get<std::string>("db_path");
    dag_processing_threads = doc.get<uint16_t>("dag_processing_threads");

    network.network_address = doc.get<std::string>("network_address");
    network.network_id = doc.get<std::string>("network_id");
    network.network_listen_port = doc.get<uint16_t>("network_listen_port");
    network.network_simulated_delay =
        doc.get<uint16_t>("network_simulated_delay");
    network.network_transaction_interval =
        doc.get<uint16_t>("network_transaction_interval");
    network.network_bandwidth = doc.get<uint16_t>("network_bandwidth");
    network.network_ideal_peer_count =
        doc.get<uint16_t>("network_ideal_peer_count");
    network.network_max_peer_count =
        doc.get<uint16_t>("network_max_peer_count");
    network.network_sync_level_size =
        doc.get<uint16_t>("network_sync_level_size");
    network.network_encrypted = doc.get<uint16_t>("network_encrypted") != 0;
    network.network_performance_log =
        doc.get<uint16_t>("network_performance_log") & 1;
    if (doc.get<uint16_t>("network_performance_log") & 2)
      dev::db::LevelDB::setPerf(true);
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
    rpc.ws_port = doc.get<uint16_t>("ws_port");
    {  // for test experiments
      for (auto &i : asVector<uint>(doc, "test_params.block_proposer")) {
        test_params.block_proposer.push_back(i);
      }

      for (auto &i : asVector<uint>(doc, "test_params.pbft")) {
        test_params.pbft.push_back(i);
      }
    }
    genesis_state = GenesisState::fromPtree(doc.get_child("genesis_state"));
    if (auto const &v = doc.get_optional<bool>("use_basic_executor"); v) {
      use_basic_executor = *v;
    }
    if (auto v = doc.get_optional<round_t>("replay_protection_service_range");
        v) {
      replay_protection_service_range = *v;
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

RpcConfig::RpcConfig(std::string const &json_file) : json_file_name(json_file) {
  try {
    boost::property_tree::ptree doc = loadJsonFile(json_file);
    port = doc.get<uint16_t>("port");
    ws_port = doc.get<uint16_t>("ws_port");
    address =
        boost::asio::ip::address::from_string(doc.get<std::string>("address"));
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
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
  strm << "  network_ideal_peer_count: " << conf.network_ideal_peer_count
       << std::endl;
  strm << "  network_max_peer_count: " << conf.network_max_peer_count
       << std::endl;
  strm << "  network_sync_level_size: " << conf.network_sync_level_size
       << std::endl;
  strm << "  network_id: " << conf.network_id << std::endl;

  strm << "  --> boot nodes  ... " << std::endl;
  for (auto const &c : conf.network_boot_nodes) {
    strm << c << std::endl;
  }
  return strm;
}

std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf) {
  strm << std::ifstream(conf.json_file_name).rdbuf() << std::endl;
  return strm;
}
}  // namespace taraxa