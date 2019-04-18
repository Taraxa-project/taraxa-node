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
#include "dag_block.hpp"
#include "full_node.hpp"
#include "libp2p/Host.h"
#include "taraxa_capability.h"
#include "transaction.hpp"
#include "util.hpp"
#include "vote.h"

namespace taraxa {

struct NodeConfig {
  std::string id;
  std::string ip;
  uint16_t port;
};

struct NetworkConfig {
  NetworkConfig(std::string const &json_file);
  std::string json_file_name;
  uint16_t network_listen_port;
  std::vector<NodeConfig> network_boot_nodes;
};

/**
 */

class Network {
 public:
  Network(std::string const &conf_file_name);
  Network(std::string const &conf_file_name, std::string networkFile);
  Network(std::string const &conf_file_name, std::string networkFile,
          secret_t const &sk);
  ~Network();
  void start();
  void stop();
  void rpcAction(boost::system::error_code const &ec, size_t size);
  void sendTest(dev::p2p::NodeID const &id);
  void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk,
                 bool newBlock);
  void sendTransactions(NodeID const &_id,
                        std::vector<Transaction> const &transactions);
  void onNewBlockVerified(DagBlock const &blk);
  void onNewTransactions(
      std::unordered_map<trx_hash_t, Transaction> const &transactions);
  NetworkConfig getConfig();
  // no need to set full node in network testing
  void setFullNode(std::shared_ptr<FullNode> full_node);
  void saveNetwork(std::string fileName);
  int getPeerCount() { return host_->peerCount(); }

  dev::p2p::NodeID getNodeId() { return host_->id(); };
  int getReceivedBlocksCount() {
    return taraxa_capability_->getBlocks().size();
  }
  int getReceivedTransactionsCount() {
    return taraxa_capability_->getTransactions().size();
  }

  // PBFT
  void noNewPbftVote(Vote const &vote);
  void sendPbftVote(NodeID const &id, Vote const &vote);

 private:
  std::shared_ptr<dev::p2p::Host> host_;
  std::shared_ptr<TaraxaCapability> taraxa_capability_;

  NetworkConfig conf_;
  bool stopped_ = true;

  std::weak_ptr<FullNode> full_node_;
  dev::Logger logger_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "network")};
  dev::Logger logger_debug_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "network")};
};

}  // namespace taraxa
#endif
