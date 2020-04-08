#include "config.hpp"

#include <json/json.h>
#include <libdevcore/LevelDB.h>

#include <fstream>

namespace taraxa {
FullNodeConfig::FullNodeConfig(std::string const &json_file)
    : json_file_name(json_file) {
  try {
    Json::Value root;
    std::ifstream config_doc(json_file, std::ifstream::binary);
    config_doc >> root;

    node_secret = root["node_secret"].asString();
    vrf_secret = root["vrf_secret"].asString();
    db_path = root["db_path"].asString();
    dag_processing_threads = root["dag_processing_threads"].asUInt();

    network.network_address = root["network_address"].asString();
    network.network_id = root["network_id"].asString();
    network.network_listen_port = root["network_listen_port"].asUInt();
    network.network_simulated_delay = root["network_simulated_delay"].asUInt();
    network.network_transaction_interval =
        root["network_transaction_interval"].asUInt();
    network.network_bandwidth = root["network_bandwidth"].asUInt();
    network.network_ideal_peer_count =
        root["network_ideal_peer_count"].asUInt();
    network.network_max_peer_count = root["network_max_peer_count"].asUInt();
    network.network_sync_level_size = root["network_sync_level_size"].asUInt();
    network.network_encrypted = root["network_encrypted"].asUInt() != 0;
    network.network_performance_log =
        root["network_performance_log"].asUInt() & 1;
    if (root["network_performance_log"].asUInt() & 2)
      dev::db::LevelDB::setPerf(true);
    for (auto &item : root["network_boot_nodes"]) {
      NodeConfig node;
      node.id = item["id"].asString();
      node.ip = item["ip"].asString();
      node.port = item["port"].asUInt();
      network.network_boot_nodes.push_back(node);
    }
    rpc.address =
        boost::asio::ip::address::from_string(network.network_address);
    rpc.port = root["rpc_port"].asUInt();
    rpc.ws_port = root["ws_port"].asUInt();
    {  // for test experiments
      test_params.max_transaction_queue_warn =
          root.get("test_params.max_transaction_queue_warn", Json::Value(0))
              .asUInt();
      test_params.max_transaction_queue_drop =
          root.get("test_params.max_transaction_queue_drop", Json::Value(0))
              .asUInt();
      test_params.max_block_queue_warn =
          root.get("test_params.max_block_queue_warn", Json::Value(0)).asUInt();
      test_params.block_proposer.mode =
          root["test_params.block_proposer.mode"].asString();
      test_params.block_proposer.shard =
          root["test_params.block_proposer.shard"].asUInt();
      test_params.block_proposer.transaction_limit =
          root["test_params.block_proposer.transaction_limit"].asUInt();
      if (test_params.block_proposer.mode == "random") {
        test_params.block_proposer.min_freq =
            root["test_params.block_proposer.random_params.min_freq"].asUInt();
        test_params.block_proposer.max_freq =
            root["test_params.block_proposer.random_params.max_freq"].asUInt();
      } else if (test_params.block_proposer.mode == "sortition") {
        test_params.block_proposer.difficulty_bound =
            root["test_params.block_proposer.sortition_params.difficulty_bound"]
                .asUInt();
        test_params.block_proposer.lambda_bits =
            root["test_params.block_proposer.sortition_params.lambda_bits"]
                .asUInt();
      } else {
        std::cerr << "Unknown propose mode: "
                  << test_params.block_proposer.mode;
        assert(false);
      }
      for (auto &i : root["test_params.pbft"]) {
        test_params.pbft.push_back(i.asInt());
      }
    }
    // TODO parse from json:
    // Either a string name of a predefined config,
    // or the full json of a custom config
    chain = decltype(chain)::DEFAULT();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

RpcConfig::RpcConfig(std::string const &json_file) : json_file_name(json_file) {
  try {
    Json::Value root;
    std::ifstream config_doc(json_file, std::ifstream::binary);
    config_doc >> root;
    port = root["port"].asUInt();
    ws_port = root["ws_port"].asUInt();
    address = boost::asio::ip::address::from_string(root["address"].asString());
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