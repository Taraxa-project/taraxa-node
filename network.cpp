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

// NetworkCongif ------------------------------

NetworkConfig::NetworkConfig(std::string const &json_file)
    : json_file_name(json_file) {
  boost::property_tree::ptree doc = loadJsonFile(json_file);
  try {
    network_listen_port = doc.get<uint16_t>("network_listen_port");
    for (auto &item : doc.get_child("network_boot_nodes")) {
      NodeConfig node;
      node.id = item.second.get<std::string>("id");
      node.ip = item.second.get<std::string>("ip");
      node.port = item.second.get<uint16_t>("port");
      network_boot_nodes.push_back(node);
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

// Network ----------------------------------------

Network::Network(std::string const &conf_file_name)
    : Network(conf_file_name, "", secret_t()) {}
Network::Network(std::string const &conf_file_name, std::string network_file)
    : Network(conf_file_name, network_file, secret_t()) {}
Network::Network(std::string const &conf_file_name, std::string network_file,
                 secret_t const &sk) try : conf_(conf_file_name) {
  auto key = dev::KeyPair::create();
  if (!sk) {
    LOG(logger_debug_) << "New key generated " << toHex(key.secret().ref());
  } else {
    key = dev::KeyPair(sk);
  }

  if (conf_file_name.empty()) {
    host_ = std::make_shared<dev::p2p::Host>(
        "TaraxaNode", key,
        dev::p2p::NetworkConfig("127.0.0.1", conf_.network_listen_port, false,
                                true));
  } else {
    auto networkData = contents(network_file);
    host_ = std::make_shared<dev::p2p::Host>(
        "TaraxaNode",
        dev::p2p::NetworkConfig("127.0.0.1", conf_.network_listen_port, false,
                                true),
        dev::bytesConstRef(&networkData));
  }
  taraxa_capability_ = std::make_shared<TaraxaCapability>(*host_.get());
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
  LOG(logger_) << "Started Node id: " << host_->id().hex();

  for (auto &node : conf_.network_boot_nodes) {
    LOG(logger_debug_) << "Adding boot node:" << node.ip << ":" << node.port;
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
  LOG(logger_debug_) << "Sent test";
}

void Network::sendBlock(NodeID const &id, DagBlock const &blk, bool newBlock) {
  taraxa_capability_->sendBlock(id, blk, newBlock);
  LOG(logger_debug_) << "Sent Block:" << blk.getHash().toString();
}

void Network::sendTransactions(NodeID const &id,
                               std::vector<Transaction> transactions) {
  taraxa_capability_->sendTransactions(id, transactions);
  LOG(logger_debug_) << "Sent transactions:" << transactions.size();
}

void Network::onNewBlock(DagBlock const &blk) {
  taraxa_capability_->onNewBlock(blk);
  LOG(logger_debug_) << "On new block:" << blk.getHash().toString();
}

void Network::onNewTransactions(std::unordered_map<trx_hash_t, Transaction> const &transactions) {
  taraxa_capability_->onNewTransactions(transactions, false);
  LOG(logger_debug_) << "On new transactions" << transactions.size();
}

void Network::saveNetwork(std::string fileName) {
  LOG(logger_debug_) << "Network saved to: " << fileName;
  auto netData = host_->saveNetwork();
  if (!netData.empty()) writeFile(fileName, &netData);
}

void Network::noNewPbftVote(Vote &vote) {
  LOG(logger_debug_) << "Network broadcast PBFT vote: " << vote.getHash();
  taraxa_capability_->onNewPbftVote(vote);
}

}  // namespace taraxa
