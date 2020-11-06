#include "network.hpp"

#include <libdevcrypto/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>

#include <boost/tokenizer.hpp>

#include "taraxa_capability.hpp"

namespace taraxa {

Network::Network(NetworkConfig const &config, std::string const &genesis, addr_t node_addr)
    : Network(config, "", secret_t(), genesis, node_addr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, public_t(), 2000) {}
Network::Network(NetworkConfig const &config, std::string const &network_file, std::string const &genesis, addr_t node_addr,
                 std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                 std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<BlockManager> blk_mgr,
                 std::shared_ptr<TransactionManager> trx_mgr, public_t node_pk, uint32_t lambda_ms_min)
    : Network(config, network_file, secret_t(), genesis, node_addr, db, pbft_mgr, pbft_chain, vote_mgr, dag_mgr, blk_mgr, trx_mgr, node_pk,
              lambda_ms_min) {}
Network::Network(NetworkConfig const &config, std::string const &network_file, secret_t const &sk, std::string const &genesis, addr_t node_addr,
                 std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                 std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<BlockManager> blk_mgr,
                 std::shared_ptr<TransactionManager> trx_mgr, public_t node_pk, uint32_t lambda_ms_min) try : conf_(config),
                                                                                                              db_(db),
                                                                                                              pbft_mgr_(pbft_mgr),
                                                                                                              pbft_chain_(pbft_chain),
                                                                                                              vote_mgr_(vote_mgr),
                                                                                                              dag_mgr_(dag_mgr),
                                                                                                              blk_mgr_(blk_mgr),
                                                                                                              trx_mgr_(trx_mgr),
                                                                                                              node_pk_(node_pk) {
  LOG_OBJECTS_CREATE("NETWORK");
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
        "TaraxaNode", conf_.network_udp_port, dev::p2p::NetworkConfig(conf_.network_address, conf_.network_tcp_port, false, true),
        dev::bytesConstRef(&networkData), conf_.network_encrypted, conf_.network_ideal_peer_count, conf_.network_max_peer_count, conf_.net_log);
  } else {
    host_ = std::make_shared<dev::p2p::Host>("TaraxaNode", key, conf_.network_udp_port,
                                             dev::p2p::NetworkConfig(conf_.network_address, conf_.network_tcp_port, false, true),
                                             conf_.network_encrypted, conf_.network_ideal_peer_count, conf_.network_max_peer_count, conf_.net_log);
  }
  taraxa_capability_ = std::make_shared<TaraxaCapability>(*host_.get(), conf_, genesis, conf_.network_performance_log, node_addr, db, pbft_mgr,
                                                          pbft_chain, vote_mgr, dag_mgr, blk_mgr, trx_mgr, lambda_ms_min);
  host_->registerCapability(taraxa_capability_);
} catch (std::exception &e) {
  std::cerr << "Construct Network Error ... " << e.what() << "\n";
  throw e;
}

Network::~Network() { stop(); }

NetworkConfig Network::getConfig() { return conf_; }

bool Network::isStarted() { return !stopped_; }

std::pair<bool, bi::tcp::endpoint> Network::resolveHost(string const &addr, uint16_t port) {
  static boost::asio::io_service s_resolverIoService;
  boost::system::error_code ec;
  bi::address address = bi::address::from_string(addr, ec);
  bi::tcp::endpoint ep(bi::address(), port);
  if (!ec)
    ep.address(address);
  else {
    boost::system::error_code ec;
    // resolve returns an iterator (host can resolve to multiple addresses)
    bi::tcp::resolver r(s_resolverIoService);
    auto it = r.resolve({bi::tcp::v4(), addr, toString(port)}, ec);
    if (ec) {
      LOG(log_er_) << "Error resolving host address... " << addr << " : " << ec.message();
      return std::make_pair(false, bi::tcp::endpoint());
    } else
      ep = *it;
  }
  return std::make_pair(true, ep);
}

void Network::start(bool boot_node) {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  assert(!host_->isStarted());
  size_t boot_node_added = 0;
  for (auto &node : conf_.network_boot_nodes) {
    if (Public(node.id) == node_pk_) continue;

    LOG(log_nf_) << "Adding boot node:" << node.ip << ":" << node.tcp_port << ":" << node.udp_port;
    if (node.ip.empty()) {
      LOG(log_wr_) << "Boot node ip is empty:" << node.ip << ":" << node.tcp_port << ":" << node.udp_port;
      continue;
    }
    if (node.tcp_port == 0 || node.tcp_port > 65535) {
      LOG(log_wr_) << "Boot node port invalid: " << node.tcp_port;
      continue;
    }
    if (node.udp_port == 0 || node.udp_port > 65535) {
      LOG(log_wr_) << "Boot node port invalid: " << node.udp_port;
      continue;
    }
    auto ip = resolveHost(node.ip, node.tcp_port);
    host_->addBootNode(dev::Public(node.id), dev::p2p::NodeIPEndpoint(ip.second.address(), node.udp_port, node.tcp_port));
    boot_node_added++;
  }
  LOG(log_nf_) << " Number of boot node added: " << boot_node_added << std::endl;
  host_->start(boot_node);
  LOG(log_nf_) << "Started Network address: " << conf_.network_address << ":" << conf_.network_tcp_port << " :" << conf_.network_udp_port
               << std::endl;
  LOG(log_nf_) << "Started Node id: " << host_->id();
}

void Network::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }

  host_->stop();
  if (!network_file_.empty()) {
    saveNetwork(network_file_);
  }
  taraxa_capability_->onStopping();
}

void Network::sendTest(NodeID const &id) {
  taraxa_capability_->sendTestMessage(id, 1);
  LOG(log_dg_) << "Sent test";
}

void Network::sendBlock(NodeID const &id, DagBlock const &blk, bool newBlock) {
  taraxa_capability_->sendBlock(id, blk, newBlock);
  LOG(log_dg_) << "Sent Block:" << blk.getHash().toString();
}

void Network::sendTransactions(NodeID const &id, std::vector<taraxa::bytes> const &transactions) {
  taraxa_capability_->sendTransactions(id, transactions);
  LOG(log_dg_) << "Sent transactions:" << transactions.size();
}

void Network::onNewBlockVerified(DagBlock const &blk) {
  taraxa_capability_->onNewBlockVerified(blk);
  LOG(log_dg_) << "On new block verified:" << blk.getHash().toString();
}

void Network::onNewTransactions(std::vector<taraxa::bytes> const &transactions) {
  taraxa_capability_->onNewTransactions(transactions, true);
  LOG(log_dg_) << "On new transactions" << transactions.size();
}

void Network::saveNetwork(std::string fileName) {
  LOG(log_dg_) << "Network saved to: " << fileName;
  auto netData = host_->saveNetwork();
  if (!netData.empty()) writeFile(fileName, &netData);
}

void Network::onNewPbftVote(Vote const &vote) {
  if (stopped_) return;
  LOG(log_dg_) << "Network broadcast PBFT vote: " << vote.getHash();
  taraxa_capability_->onNewPbftVote(vote);
}

void Network::sendPbftVote(NodeID const &id, Vote const &vote) {
  if (stopped_) return;
  LOG(log_dg_) << "Network sent PBFT vote: " << vote.getHash() << " to: " << id;
  taraxa_capability_->sendPbftVote(id, vote);
}

void Network::onNewPbftBlock(const taraxa::PbftBlock &pbft_block) {
  if (stopped_) return;
  LOG(log_dg_) << "Network broadcast PBFT block: " << pbft_block.getBlockHash();
  taraxa_capability_->onNewPbftBlock(pbft_block);
}

void Network::sendPbftBlock(const NodeID &id, const taraxa::PbftBlock &pbft_block, uint64_t const &pbft_chain_size) {
  LOG(log_dg_) << "Network send PBFT block: " << pbft_block.getBlockHash() << " to: " << id;
  taraxa_capability_->sendPbftBlock(id, pbft_block, pbft_chain_size);
}

}  // namespace taraxa
