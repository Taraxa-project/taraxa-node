/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-12-11 16:03:02
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:13:00
 */

#ifndef NETWORK_HPP
#define NETWORK_HPP
#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>
#include <boost/thread.hpp>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include "dag_block.hpp"
#include "full_node.hpp"
#include "libp2p/Host.h"
#include "taraxa_capability.h"
#include "util.hpp"

namespace taraxa {

#define NETWORK_GLOBAL_LOGGER(NAME, SEVERITY)                      \
    BOOST_LOG_INLINE_GLOBAL_LOGGER_CTOR_ARGS(g_##NAME##Logger, \
        boost::log::sources::severity_channel_logger_mt<>,     \
        (boost::log::keywords::severity = SEVERITY)(boost::log::keywords::channel = "network"))

NETWORK_GLOBAL_LOGGER(networknote, VerbosityInfo)
#define cnetworknote LOG(taraxa::g_networknoteLogger::get())
NETWORK_GLOBAL_LOGGER(networklog, VerbosityDebug)
#define cnetworklog LOG(taraxa::g_networklogLogger::get())
NETWORK_GLOBAL_LOGGER(networkdetails, VerbosityTrace)
#define cnetworkdetails LOG(taraxa::g_networkdetailsLogger::get())


struct NodeConfig {
  std::string id;
  std::string ip;
  uint16_t port;
};

struct NetworkConfig {
  NetworkConfig(std::string const &json_file);
  std::string json_file_name;
  uint16_t network_listen_port;
  std::string network_node_id;
  std::vector<NodeConfig> network_boot_nodes;
  std::string log_verbosity;
  std::string log_channels;
};

/**
 */

class Network {
 public:
  Network(std::string const &conf_file_name);
  Network(std::string const &conf_file_name, std::string networkFile);
  ~Network();
  void start();
  void stop();
  void rpcAction(boost::system::error_code const &ec, size_t size);
  void sendTest(dev::p2p::NodeID const &id);
  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk,
                 bool newBlock);
  void onNewBlock(DagBlock const &blk);
  NetworkConfig getConfig();
  // no need to set full node in network testing
  void setFullNode(std::shared_ptr<FullNode> full_node);
  void saveNetwork(std::string fileName);
  int getPeerCount() { return host_->peerCount(); }

  dev::p2p::NodeID getNodeId() { return host_->id(); };
  int getReceivedBlocksCount() {
    return taraxa_capability_->getBlocks().size();
  }

 private:
  std::shared_ptr<dev::p2p::Host> host_;
  std::shared_ptr<TaraxaCapability> taraxa_capability_;

  NetworkConfig conf_;
  bool stopped_ = true;

  std::weak_ptr<FullNode> full_node_;
};

}  // namespace taraxa
#endif
