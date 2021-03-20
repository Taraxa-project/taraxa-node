#include "network.hpp"

#include <libdevcrypto/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>

#include <boost/tokenizer.hpp>

#include "taraxa_capability.hpp"
#include "util/boost_asio.hpp"

namespace taraxa {

std::pair<bool, bi::tcp::endpoint> resolveHost(string const &addr, uint16_t port) {
  static boost::asio::io_service s_resolverIoService;
  boost::system::error_code ec;
  bi::address address = bi::address::from_string(addr, ec);
  bi::tcp::endpoint ep(bi::address(), port);
  if (!ec) {
    ep.address(address);
  } else {
    boost::system::error_code ec;
    // resolve returns an iterator (host can resolve to multiple addresses)
    bi::tcp::resolver r(s_resolverIoService);
    auto it = r.resolve({bi::tcp::v4(), addr, toString(port)}, ec);
    if (ec) {
      return std::make_pair(false, bi::tcp::endpoint());
    } else {
      ep = *it;
    }
  }
  return std::make_pair(true, ep);
}

Network::Network(NetworkConfig const &config, std::filesystem::path const &network_file_path, dev::KeyPair const &key,
                 std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr,
                 std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                 std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr,
                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                 std::shared_ptr<TransactionManager> trx_mgr)
    : conf_(config), tp_(config.network_num_threads, false), network_file_(network_file_path) {
  auto const &node_addr = key.address();
  LOG_OBJECTS_CREATE("NETWORK");

  LOG(log_nf_) << "Read Network Config: " << std::endl << conf_ << std::endl;

  // TODO make all these properties configurable
  dev::p2p::NetworkConfig net_conf;
  net_conf.listenIPAddress = conf_.network_address;
  net_conf.listenPort = conf_.network_tcp_port;
  net_conf.discovery = true;
  net_conf.allowLocalDiscovery = true;
  net_conf.traverseNAT = false;
  net_conf.publicIPAddress = {};
  net_conf.pin = false;

  TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.ideal_peer_count = conf_.network_ideal_peer_count;
  // TODO conf_.network_max_peer_count -> conf_.peer_count_stretch
  taraxa_net_conf.peer_stretch = conf_.network_max_peer_count / conf_.network_ideal_peer_count;
  taraxa_net_conf.is_boot_node = conf_.network_is_boot_node;
  for (auto const &node : conf_.network_boot_nodes) {
    Public pub(node.id);
    if (pub == key.pub()) {
      LOG(log_wr_) << "not adding self to the boot node list";
      continue;
    }
    LOG(log_nf_) << "Adding boot node:" << node.ip << ":" << node.tcp_port;
    auto ip = resolveHost(node.ip, node.tcp_port);
    boot_nodes_[pub] = dev::p2p::NodeIPEndpoint(ip.second.address(), node.tcp_port, node.tcp_port);
  }
  LOG(log_nf_) << " Number of boot node added: " << boot_nodes_.size() << std::endl;
  string net_version = "TaraxaNode";  // TODO maybe give a proper name?
  dev::bytes networkData;
  if (!network_file_.empty()) {
    networkData = contents(network_file_.string());
  }
  if (!networkData.empty()) {
    host_ = std::make_shared<dev::p2p::Host>(net_version, net_conf, move(taraxa_net_conf), &networkData);
  } else {
    host_ = std::make_shared<dev::p2p::Host>(net_version, makeENR(key, net_conf), net_conf, move(taraxa_net_conf));
  }
  taraxa_capability_ =
      std::make_shared<TaraxaCapability>(*host_, tp_.unsafe_get_io_context(), conf_, db, pbft_mgr, pbft_chain, vote_mgr, next_votes_mgr,
                                         dag_mgr, dag_blk_mgr, trx_mgr, conf_.network_performance_log, key.address());
  host_->registerCapability(taraxa_capability_);
}

Network::~Network() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  tp_.stop();
  host_->stop();
  if (!network_file_.empty()) {
    if (auto netData = host_->saveNetwork(); !netData.empty()) {
      writeFile(network_file_.string(), &netData);
      LOG(log_dg_) << "Network saved to: " << network_file_;
    }
  }
}

NetworkConfig Network::getConfig() { return conf_; }

bool Network::isStarted() { return !stopped_; }

void Network::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  assert(!host_->isStarted());
  host_->start();
  if (!boot_nodes_.empty()) {
    for (auto const &[k, v] : boot_nodes_) {
      host_->addNode(k, v);
    }
    // Every 30 seconds check if connected to another node and refresh boot nodes
    util::post_periodic(tp_.unsafe_get_io_context(), {30000}, [this] {
      // If node count drops to zero add boot nodes again and retry
      if (getNodeCount() == 0) {
        for (auto const &[k, v] : boot_nodes_) {
          host_->addNode(k, v);
        }
      }
      if (host_->peerCount() == 0) {
        for (auto const &[k, _] : boot_nodes_) {
          host_->invalidateNode(k);
        }
      }
    });
  }
  tp_.start();
  LOG(log_nf_) << "Started Network address: " << conf_.network_address << ":" << conf_.network_tcp_port << std::endl;
  LOG(log_nf_) << "Started Node id: " << host_->id();
}

void Network::sendBlock(NodeID const &id, DagBlock const &blk) {
  taraxa_capability_->sendBlock(id, blk);
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

void Network::sendPbftBlock(const NodeID &id, const taraxa::PbftBlock &pbft_block, uint64_t const &pbft_chain_size) {
  LOG(log_dg_) << "Network send PBFT block: " << pbft_block.getBlockHash() << " to: " << id;
  taraxa_capability_->sendPbftBlock(id, pbft_block, pbft_chain_size);
}

std::pair<dev::Secret, dev::p2p::ENR> Network::makeENR(KeyPair const &key, p2p::NetworkConfig const &net_conf) {
  return pair{
      key.secret(),
      IdentitySchemeV4::createENR(
          key.secret(), net_conf.publicIPAddress.empty() ? bi::address{} : bi::make_address(net_conf.publicIPAddress),
          net_conf.listenPort, net_conf.listenPort),
  };
}

void Network::broadcastPreviousRoundNextVotesBundle() {
  LOG(log_dg_) << "Network broadcast previous round next votes bundle";
  taraxa_capability_->broadcastPreviousRoundNextVotesBundle();
}

}  // namespace taraxa
