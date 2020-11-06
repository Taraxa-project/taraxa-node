#ifndef TARAXA_NODE_CONFIG_HPP
#define TARAXA_NODE_CONFIG_HPP

#include <boost/log/attributes/clock.hpp>
#include <boost/log/attributes/function.hpp>
#include <boost/log/detail/sink_init_helpers.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/channel_feature.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <string>

#include "chain_config.hpp"
#include "dag_block.hpp"
#include "log.hpp"
#include "types.hpp"
#include "util.hpp"

namespace taraxa {

using Logger = boost::log::sources::severity_channel_logger<>;
template <class T>
using log_sink = boost::log::sinks::synchronous_sink<T>;

struct ConfigException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct RpcConfig {
  optional<uint16_t> port;
  optional<uint16_t> ws_port;
  boost::asio::ip::address address;
};

struct NodeConfig {
  std::string id;
  std::string ip;
  uint16_t tcp_port = 0;
  uint16_t udp_port = 0;
};

struct NetworkConfig {
  std::string json_file_name;
  bool network_is_boot_node = 0;
  std::string network_address;
  uint16_t network_tcp_port = 0;
  uint16_t network_udp_port = 0;
  std::vector<NodeConfig> network_boot_nodes;
  uint16_t network_simulated_delay = 0;
  uint16_t network_bandwidth = 0;
  uint16_t network_ideal_peer_count = 0;
  uint16_t network_max_peer_count = 0;
  uint16_t network_transaction_interval = 0;
  uint16_t network_min_dag_block_broadcast = 0;
  uint16_t network_max_dag_block_broadcast = 0;
  uint16_t network_sync_level_size = 0;
  uint64_t network_id;
  bool network_encrypted = 0;
  bool network_performance_log = 0;
  bool net_log = 0;
};

struct LoggingOutputConfig {
  LoggingOutputConfig() = default;
  std::string type = "console";
  std::string file_name;
  uint64_t rotation_size = 0;
  std::string time_based_rotation;
  std::string format = "%NodeId% %Channel% [%TimeStamp%] %SeverityStr%: %Message%";
  uint64_t max_size = 0;
};

struct LoggingConfig {
  LoggingConfig() = default;
  std::string name;
  Verbosity verbosity = Verbosity::VerbosityError;
  std::map<std::string, uint16_t> channels;
  std::vector<LoggingOutputConfig> outputs;
  std::vector<boost::shared_ptr<log_sink<boost::log::sinks::text_ostream_backend>>> console_sinks;
  std::vector<boost::shared_ptr<log_sink<boost::log::sinks::text_file_backend>>> file_sinks;
};

struct BlockProposerConfig {
  uint16_t shard = 0;
  uint16_t transaction_limit = 0;
};

// Parameter Tuning purpose
struct TestParamsConfig {
  BlockProposerConfig block_proposer;  // test_params.block_proposer
  uint32_t max_transaction_queue_warn = 0;
  uint32_t max_transaction_queue_drop = 0;
  uint32_t max_block_queue_warn = 0;
  uint32_t db_snapshot_each_n_pbft_block = 0;
  uint32_t db_max_snapshots = 0;
  uint32_t db_revert_to_period = 0;
};

struct FullNodeConfig {
  FullNodeConfig() = default;
  // The reason of using Json::Value as a union is that in the tests
  // there are attempts to pass char const* to this constructor, which
  // is ambiguous (char const* may promote to Json::Value)
  // if you have std::string and Json::Value constructor. It was easier
  // to just treat Json::Value as a std::string or Json::Value depending on
  // the contents
  explicit FullNodeConfig(Json::Value const &file_name_str_or_json_object);
  std::string json_file_name;
  std::string node_secret;
  vrf_wrapper::vrf_sk_t vrf_secret;
  fs::path db_path;
  NetworkConfig network;
  RpcConfig rpc;
  TestParamsConfig test_params;
  ChainConfig chain = ChainConfig::predefined();
  FinalChain::Opts opts_final_chain;
  std::vector<LoggingConfig> log_configs;

  auto net_file_path() const { return db_path / "net"; }
};

std::ostream &operator<<(std::ostream &strm, NodeConfig const &conf);
std::ostream &operator<<(std::ostream &strm, NetworkConfig const &conf);
std::ostream &operator<<(std::ostream &strm, FullNodeConfig const &conf);

}  // namespace taraxa

#endif
