#pragma once

#include <json/json.h>

#include <string>

#include "common/types.hpp"

namespace taraxa {

struct PrometheusConfig {
  std::string address;
  uint16_t listen_port = 0;
  uint16_t polling_interval_ms = 1000;
};

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

struct DdosProtectionConfig {
  // How many periods(of votes) into the future compared to the current state we accept
  PbftPeriod vote_accepting_periods{5};
  // How many rounds(of votes) into the future compared to the current state we accept
  PbftRound vote_accepting_rounds{5};
  // How many steps(of votes) into the future compared to the current state we accept
  PbftStep vote_accepting_steps{0};

  // Time period for collecting packets stats
  std::chrono::milliseconds packets_stats_time_period_ms{60000};
  // Log packets stats flag
  bool log_packets_stats = false;
  // Peer's max allowed packets processing time during packets_stats_time_period_ms
  std::chrono::microseconds peer_max_packets_processing_time_us{0};
  // Queue size limit when we start dropping packets from peers if they exceed peer_max_packets_processing_time_us
  uint64_t peer_max_packets_queue_size_limit{0};

  // Max packets queue size, 0 means unlimited
  uint64_t max_packets_queue_size{0};

  void validate(uint32_t delegation_delay) const;
  bool isPeerPacketsProtectionEnabled() const;
};
DdosProtectionConfig dec_ddos_protection_config_json(const Json::Value &json);

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
  uint16_t num_threads = std::max(uint(1), uint(std::thread::hardware_concurrency() / 2));
  uint16_t packets_processing_threads = 14;
  uint16_t peer_blacklist_timeout = kBlacklistTimeoutDefaultInSeconds;
  bool disable_peer_blacklist = false;
  uint16_t deep_syncing_threshold = 10;
  DdosProtectionConfig ddos_protection;

  std::optional<ConnectionConfig> rpc;
  std::optional<ConnectionConfig> graphql;
  std::optional<PrometheusConfig> prometheus;

  void validate(uint32_t delegation_delay) const;
};

void dec_json(const Json::Value &json, NetworkConfig &network);

}  // namespace taraxa