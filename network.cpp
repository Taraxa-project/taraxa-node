#include "network.hpp"
#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Network.h>
#include <boost/tokenizer.hpp>
#include "full_node.hpp"
#include "libp2p/Host.h"
#include "taraxa_capability.h"
#include "visitor.hpp"

namespace taraxa {

Network::Network(NetworkConfig const &config)
    : Network(config, "", secret_t()) {}
Network::Network(NetworkConfig const &config, std::string network_file)
    : Network(config, network_file, secret_t()) {}
Network::Network(NetworkConfig const &config, std::string network_file,
                 secret_t const &sk) try : conf_(config) {
  auto key = dev::KeyPair::create();
  if (!sk) {
    LOG(log_dg_) << "New key generated " << toHex(key.secret().ref());
  } else {
    key = dev::KeyPair(sk);
  }

  if(network_file != "") {
    auto networkData = contents(network_file);
    host_ = std::make_shared<dev::p2p::Host>(
        "TaraxaNode",
        dev::p2p::NetworkConfig(conf_.network_address, conf_.network_listen_port,
                                false, true),
        dev::bytesConstRef(&networkData));
  }
  else {
    host_ = std::make_shared<dev::p2p::Host>(
        "TaraxaNode", key,
        dev::p2p::NetworkConfig(conf_.network_address, conf_.network_listen_port,
                                false, true));
  }
  taraxa_capability_ = std::make_shared<TaraxaCapability>(*host_.get(), conf_.network_simulated_delay);
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
void Network::start() {
  if (!stopped_) {
    return;
  }
  stopped_ = false;
  host_->start();
  LOG(log_si_) << "Started Network address: " << conf_.network_address << ":" << conf_.network_listen_port
               << std::endl;
  LOG(log_si_) << "Started Node id: " << host_->id();

  for (auto &node : conf_.network_boot_nodes) {
    LOG(log_nf_) << "Adding boot node:" << node.ip << ":" << node.port;
    host_->addNode(
        dev::Public(node.id),
        dev::p2p::NodeIPEndpoint(bi::address::from_string(node.ip.c_str()),
                                 node.port, node.port));
  }
}

void Network::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;
  host_->stop();
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
                               std::vector<Transaction> const &transactions) {
  taraxa_capability_->sendTransactions(id, transactions);
  LOG(log_dg_) << "Sent transactions:" << transactions.size();
}

void Network::onNewBlockVerified(DagBlock const &blk) {
  taraxa_capability_->onNewBlockVerified(blk);
  LOG(log_dg_) << "On new block verified:" << blk.getHash().toString();
}

void Network::onNewTransactions(
    std::unordered_map<trx_hash_t, Transaction> const &transactions) {
  taraxa_capability_->onNewTransactions(transactions, true);
  LOG(log_dg_) << "On new transactions" << transactions.size();
}

void Network::saveNetwork(std::string fileName) {
  LOG(log_dg_) << "Network saved to: " << fileName;
  auto netData = host_->saveNetwork();
  if (!netData.empty()) writeFile(fileName, &netData);
}

void Network::onNewPbftVote(Vote const &vote) {
  LOG(log_dg_) << "Network broadcast PBFT vote: "
                     << vote.getHash().toString();
  taraxa_capability_->onNewPbftVote(vote);
}

void Network::sendPbftVote(NodeID const &id, Vote const &vote) {
  LOG(log_dg_) << "Network sent PBFT vote: " << vote.getHash().toString()
               << " to: " << id;
  taraxa_capability_->sendPbftVote(id, vote);
}

}  // namespace taraxa
