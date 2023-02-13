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

Json::Value ConnectionConfig::toJson() const {
  Json::Value json_config;

  json_config["enabled"] = enabled;
  if (http_port.has_value()) {
    json_config["http_port"] = *http_port;
  }
  if (ws_port.has_value()) {
    json_config["ws_port"] = *ws_port;
  }
  json_config["threads_num"] = threads_num;

  return json_config;
}

Json::Value NodeConfig::toJson() const {
  Json::Value json_config;

  json_config["id"] = id;
  json_config["ip"] = ip;
  json_config["port"] = port;

  return json_config;
}

void PrometheusConfig::validate() const {
  if (enabled && !listen_port) {
    throw ConfigException("listen_port must be specified for prometheus config");
  }
}

Json::Value PrometheusConfig::toJson() const {
  Json::Value json_config;

  json_config["enabled"] = enabled;
  json_config["listen_port"] = listen_port;
  json_config["polling_interval_ms"] = polling_interval_ms;

  return json_config;
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
  for (const auto& node : boot_nodes) {
    if (node.ip.empty()) {
      throw ConfigException(std::string("Boot node ip is empty:") + node.ip + ":" + std::to_string(node.port));
    }
    if (node.port == 0) {
      throw ConfigException(std::string("Boot node port invalid: ") + std::to_string(node.port));
    }
  }
}

Json::Value NetworkConfig::toJson() const {
  Json::Value json_config;

  json_config["public_ip"] = public_ip;
  json_config["listen_ip"] = listen_ip;
  json_config["listen_port"] = listen_port;
  json_config["ideal_peer_count"] = ideal_peer_count;
  json_config["max_peer_count"] = max_peer_count;
  json_config["transaction_interval_ms"] = transaction_interval_ms;
  json_config["sync_level_size"] = sync_level_size;
  json_config["num_threads"] = num_threads;
  json_config["packets_processing_threads"] = packets_processing_threads;
  json_config["peer_blacklist_timeout"] = peer_blacklist_timeout;
  json_config["disable_peer_blacklist"] = disable_peer_blacklist;
  json_config["deep_syncing_threshold"] = deep_syncing_threshold;
  json_config["ddos_protection"] = ddos_protection.toJson();

  json_config["rpc"] = rpc.toJson();
  json_config["graphql"] = graphql.toJson();
  json_config["prometheus"] = prometheus.toJson();

  auto& boot_nodes_json = json_config["boot_nodes"] = Json::Value(Json::arrayValue);
  for (const auto& boot_node : boot_nodes) {
    boot_nodes_json.append(boot_node.toJson());
  }

  return json_config;
}

Json::Value DdosProtectionConfig::toJson() const {
  Json::Value json_config;

  json_config["vote_accepting_periods"] = vote_accepting_periods;
  json_config["vote_accepting_rounds"] = vote_accepting_rounds;
  json_config["vote_accepting_steps"] = vote_accepting_steps;
  json_config["packets_stats_time_period_ms"] = packets_stats_time_period_ms.count();
  json_config["log_packets_stats"] = log_packets_stats;
  json_config["peer_max_packets_processing_time_us"] = peer_max_packets_processing_time_us.count();
  json_config["peer_max_packets_queue_size_limit"] = peer_max_packets_queue_size_limit;
  json_config["max_packets_queue_size"] = max_packets_queue_size;

  return json_config;
}

}  // namespace taraxa