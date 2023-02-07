#include "config/network.hpp"

#include "config/config_utils.hpp"

namespace taraxa {

void ConnectionConfig::validate() const {
  if (enabled && !http_port.has_value() && !ws_port.has_value()) {
    throw ConfigException("Either http_port or ws_port post must be specified for connection config");
  }

  // Max enabled number of threads for processing requests
  constexpr uint16_t kMaxThreadsNum = 10;
  if (threads_num <= 0 || threads_num > kMaxThreadsNum) {
    throw ConfigException(std::string("threads_num must be in range (0, ") + std::to_string(kMaxThreadsNum) + "]");
  }
}

void PrometheusConfig::validate() const {
  if (enabled && !listen_port) {
    throw ConfigException("listen_port must be specified for prometheus config");
  }
}

void DdosProtectionConfig::validate(uint32_t delegation_delay) const {
  if (vote_accepting_periods > delegation_delay) {
    throw ConfigException(std::string("network.ddos_protection.vote_accepting_periods(" +
                                      std::to_string(vote_accepting_periods) + ") must be <= DPOS.delegation_delay(" +
                                      std::to_string(delegation_delay) + ")"));
  }

  if (peer_max_packets_queue_size_limit && peer_max_packets_processing_time_us.count() == 0) {
    throw ConfigException(
        std::string("if network.ddos_protection.peer_max_packets_queue_size_limit != 0 then "
                    "network.ddos_protection.peer_max_packets_processing_time_us must be != 0 too"));
  }

  if ((log_packets_stats || peer_max_packets_queue_size_limit) && packets_stats_time_period_ms.count() == 0) {
    throw ConfigException(
        std::string("if network.ddos_protection.peer_max_packets_queue_size_limit != 0 or "
                    "network.ddos_protection.log_packets_stats == true then "
                    "network.ddos_protection.packets_stats_time_period_ms must be != 0 too"));
  }
}

DdosProtectionConfig dec_ddos_protection_config_json(const Json::Value &json) {
  DdosProtectionConfig ddos_protection;
  ddos_protection.vote_accepting_periods = getConfigDataAsUInt(json, {"vote_accepting_periods"});
  ddos_protection.vote_accepting_rounds = getConfigDataAsUInt(json, {"vote_accepting_rounds"});
  ddos_protection.vote_accepting_steps = getConfigDataAsUInt(json, {"vote_accepting_steps"});
  ddos_protection.log_packets_stats = getConfigDataAsBoolean(json, {"log_packets_stats"});

  ddos_protection.packets_stats_time_period_ms =
      std::chrono::milliseconds{getConfigDataAsUInt(json, {"packets_stats_time_period_ms"})};
  ddos_protection.peer_max_packets_processing_time_us =
      std::chrono::microseconds{getConfigDataAsUInt64(json, {"peer_max_packets_processing_time_us"})};
  ddos_protection.peer_max_packets_queue_size_limit =
      getConfigDataAsUInt64(json, {"peer_max_packets_queue_size_limit"});
  ddos_protection.max_packets_queue_size = getConfigDataAsUInt64(json, {"max_packets_queue_size"});
  return ddos_protection;
}

void NetworkConfig::validate(uint32_t delegation_delay) const {
  rpc.validate();
  graphql.validate();
  prometheus.validate();

  ddos_protection.validate(delegation_delay);

  if (sync_level_size == 0) {
    throw ConfigException(std::string("network.sync_level_size cannot be 0"));
  }

  // Max enabled number of threads for processing rpc requests
  constexpr uint16_t MAX_PACKETS_PROCESSING_THREADS_NUM = 30;
  if (packets_processing_threads < 3 || packets_processing_threads > MAX_PACKETS_PROCESSING_THREADS_NUM) {
    throw ConfigException(std::string("network.packets_processing_threads must be in range [3, ") +
                          std::to_string(MAX_PACKETS_PROCESSING_THREADS_NUM) + "]");
  }

  if (transaction_interval_ms == 0) {
    throw ConfigException(std::string("network.transaction_interval_ms must be greater than zero"));
  }

  // TODO validate that the boot node list doesn't contain self (although it's not critical)
  for (const auto &node : boot_nodes) {
    if (node.ip.empty()) {
      throw ConfigException(std::string("Boot node ip is empty:") + node.ip + ":" + std::to_string(node.port));
    }
    if (node.port == 0) {
      throw ConfigException(std::string("Boot node port invalid: ") + std::to_string(node.port));
    }
  }
}

NodeConfig dec_json(const Json::Value &json) {
  NodeConfig node;
  node.id = getConfigDataAsString(json, {"id"});
  node.ip = getConfigDataAsString(json, {"ip"});
  node.port = getConfigDataAsUInt(json, {"port"});
  return node;
}

void dec_json(const Json::Value &json, NetworkConfig &network) {
  network.listen_ip = getConfigDataAsString(json, {"listen_ip"});
  network.public_ip = getConfigDataAsString(json, {"public_ip"}, true);
  network.listen_port = getConfigDataAsUInt(json, {"listen_port"});
  network.transaction_interval_ms = getConfigDataAsUInt(json, {"transaction_interval_ms"});
  network.ideal_peer_count = getConfigDataAsUInt(json, {"ideal_peer_count"});
  network.max_peer_count = getConfigDataAsUInt(json, {"max_peer_count"});
  network.sync_level_size = getConfigDataAsUInt(json, {"sync_level_size"});
  network.packets_processing_threads = getConfigDataAsUInt(json, {"packets_processing_threads"});
  network.peer_blacklist_timeout =
      getConfigDataAsUInt(json, {"peer_blacklist_timeout"}, true, NetworkConfig::kBlacklistTimeoutDefaultInSeconds);
  network.disable_peer_blacklist = getConfigDataAsBoolean(json, {"disable_peer_blacklist"}, true, false);
  network.deep_syncing_threshold =
      getConfigDataAsUInt(json, {"deep_syncing_threshold"}, true, network.deep_syncing_threshold);
  network.ddos_protection = dec_ddos_protection_config_json(getConfigData(json, {"ddos_protection"}));

  for (auto &item : json["boot_nodes"]) {
    network.boot_nodes.push_back(dec_json(item));
  }
}

}  // namespace taraxa