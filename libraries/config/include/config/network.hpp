#pragma once

#include <json/json.h>

#include <string>

#include "common/types.hpp"

namespace taraxa {

struct ConnectionConfig {
  std::optional<uint16_t> http_port;
  std::optional<uint16_t> ws_port;
  boost::asio::ip::address address;

  // Number of threads dedicated to the rpc calls processing, default = 5
  uint16_t threads_num{5};

  void validate() const;
};

void dec_json(const Json::Value &json, ConnectionConfig &config);

struct NodeConfig {
  std::string id;
  std::string ip;
  uint16_t port = 0;
};

struct NetworkConfig {
  static constexpr uint16_t kBlacklistTimeoutDefaultInSeconds = 600;

  std::string json_file_name;
  std::string public_ip;
  std::string listen_ip = "127.0.0.1";
  uint16_t listen_port = 0;
  std::vector<NodeConfig> boot_nodes;
  uint16_t ideal_peer_count = 10;
  uint16_t max_peer_count = 50;
  uint16_t transaction_interval_ms = 100;
  uint16_t sync_level_size = 10;
  bool collect_packets_stats = false;
  uint16_t num_threads = std::max(uint(1), uint(std::thread::hardware_concurrency() / 2));
  uint16_t packets_processing_threads = 14;
  uint16_t peer_blacklist_timeout = kBlacklistTimeoutDefaultInSeconds;
  bool disable_peer_blacklist = false;
  uint16_t deep_syncing_threshold = 10;
  PbftPeriod vote_accepting_periods = 5;
  PbftRound vote_accepting_rounds = 5;
  PbftStep vote_accepting_steps = 0;

  std::optional<ConnectionConfig> rpc;
  std::optional<ConnectionConfig> graphql;

  void validate() const;
};

void dec_json(const Json::Value &json, NetworkConfig &network);

}  // namespace taraxa