#include "network.hpp"
#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Network.h>
#include <boost/tokenizer.hpp>
#include "full_node.hpp"
#include "libp2p/Host.h"
#include "taraxa_capability.hpp"

namespace taraxa {

Network::Network(NetworkConfig const &config, std::string const &genesis)
    : Network(config, "", secret_t(), genesis) {}
Network::Network(NetworkConfig const &config, std::string network_file,
                 std::string const &genesis)
    : Network(config, network_file, secret_t(), genesis) {}
Network::Network(NetworkConfig const &config, std::string network_file,
                 secret_t const &sk, std::string const &genesis) try
    : conf_(config) {
  LOG(log_nf_) << "Read Network Config: " << std::endl << conf_ << std::endl;
  auto key = dev::KeyPair::create();
  if (!sk) {
    LOG(log_dg_) << "New key generated " << toHex(key.secret().ref());
  } else {
    key = dev::KeyPair(sk);
  }

  dev::bytes networkData;
  if (network_file != "") {
    network_file_ = network_file;
    networkData = contents(network_file);
  }
  if (networkData.size() > 0) {
    host_ = std::make_shared<dev::p2p::Host>(
        "TaraxaNode",
        dev::p2p::NetworkConfig(conf_.network_address,
                                conf_.network_listen_port, false, true),
        dev::bytesConstRef(&networkData), conf_.network_encrypted);
  } else {
    host_ = std::make_shared<dev::p2p::Host>(
        "TaraxaNode", key,
        dev::p2p::NetworkConfig(conf_.network_address,
                                conf_.network_listen_port, false, true),
        conf_.network_encrypted);
  }
  taraxa_capability_ = std::make_shared<TaraxaCapability>(
      *host_.get(), conf_, genesis, conf_.network_performance_log);
  host_->registerCapability(taraxa_capability_);
} catch (std::exception &e) {
  std::cerr << "Construct Network Error ... " << e.what() << "\n";
  throw e;
}

Network::~Network() {
  if (!stopped_) {
    stop();
  }
}

void Network::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
  taraxa_capability_->setFullNode(full_node);
}

NetworkConfig Network::getConfig() { return conf_; }

bool Network::isStarted() { return !stopped_; }

void Network::start(bool boot_node) {
  if (!stopped_) {
    return;
  }
  stopped_ = false;

  if (host_->isStarted()) {
    return;
  }
  host_->start(boot_node);
  LOG(log_nf_) << "Started Network address: " << conf_.network_address << ":"
               << conf_.network_listen_port << std::endl;
  LOG(log_nf_) << "Started Node id: " << host_->id();
  size_t boot_node_added = 0;
  for (auto &node : conf_.network_boot_nodes) {
    if (auto full_node = full_node_.lock()) {
      if (Public(node.id) == full_node->getPublicKey()) continue;
    }

    LOG(log_nf_) << "Adding boot node:" << node.ip << ":" << node.port;
    if (node.ip.empty()) {
      LOG(log_wr_) << "Boot node ip is empty:" << node.ip << ":" << node.port;
      continue;
    }
    if (node.port <= 0 || node.port > 65535) {
      LOG(log_wr_) << "Boot node port invalid: " << node.port;
      continue;
    }
    host_->addNode(
        dev::Public(node.id),
        dev::p2p::NodeIPEndpoint(bi::address::from_string(node.ip.c_str()),
                                 node.port, node.port));
    boot_node_added++;
  }
  LOG(log_nf_) << " Number of boot node added: " << boot_node_added
               << std::endl;
}

void Network::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;
  host_->stop();
  if (network_file_ != "") saveNetwork(network_file_);
}

void Network::sendTest(NodeID const &id) {
  taraxa_capability_->sendTestMessage(id, 1);
  LOG(log_dg_) << "Sent test";
}

void Network::sendBlock(NodeID const &id, DagBlock const &blk, bool newBlock) {
  taraxa_capability_->sendBlock(id, blk, newBlock);
  LOG(log_dg_) << "Sent Block:" << blk.getHash().toString();
}

void Network::sendTransactions(NodeID const &id,
                               std::vector<taraxa::bytes> const &transactions) {
  taraxa_capability_->sendTransactions(id, transactions);
  LOG(log_dg_) << "Sent transactions:" << transactions.size();
}

void Network::onNewBlockVerified(DagBlock const &blk) {
  taraxa_capability_->onNewBlockVerified(blk);
  LOG(log_dg_) << "On new block verified:" << blk.getHash().toString();
}

void Network::onNewTransactions(
    std::unordered_map<trx_hash_t, std::pair<Transaction, taraxa::bytes>> const &transactions) {
  taraxa_capability_->onNewTransactions(transactions, true);
  LOG(log_dg_) << "On new transactions" << transactions.size();
}

void Network::saveNetwork(std::string fileName) {
  LOG(log_dg_) << "Network saved to: " << fileName;
  auto netData = host_->saveNetwork();
  if (!netData.empty()) writeFile(fileName, &netData);
}

void Network::onNewPbftVote(Vote const &vote) {
  LOG(log_dg_) << "Network broadcast PBFT vote: " << vote.getHash();
  taraxa_capability_->onNewPbftVote(vote);
}

void Network::sendPbftVote(NodeID const &id, Vote const &vote) {
  LOG(log_dg_) << "Network sent PBFT vote: " << vote.getHash() << " to: " << id;
  taraxa_capability_->sendPbftVote(id, vote);
}

void Network::onNewPbftBlock(const taraxa::PbftBlock &pbft_block) {
  LOG(log_dg_) << "Network broadcast PBFT block: " << pbft_block.getBlockHash();
  taraxa_capability_->onNewPbftBlock(pbft_block);
}

void Network::sendPbftBlock(const NodeID &id,
                            const taraxa::PbftBlock &pbft_block) {
  LOG(log_dg_) << "Network send PBFT block: " << pbft_block.getBlockHash()
               << " to: " << id;
  taraxa_capability_->sendPbftBlock(id, pbft_block);
}

}  // namespace taraxa
