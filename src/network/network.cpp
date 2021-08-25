#include "network.hpp"

#include <libdevcrypto/Common.h>
#include <libp2p/Host.h>
#include <libp2p/Network.h>

#include <boost/tokenizer.hpp>

namespace taraxa {

Network::Network(NetworkConfig const &config, std::filesystem::path const &network_file_path, dev::KeyPair const &key,
                 std::shared_ptr<DbStorage> db, std::shared_ptr<PbftManager> pbft_mgr,
                 std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                 std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr, std::shared_ptr<DagManager> dag_mgr,
                 std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<TransactionManager> trx_mgr)
    : conf_(config), tp_(config.network_num_threads, false) {
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
  dev::p2p::TaraxaNetworkConfig taraxa_net_conf;
  taraxa_net_conf.ideal_peer_count = conf_.network_ideal_peer_count;

  // TODO conf_.network_max_peer_count -> conf_.peer_count_stretch
  taraxa_net_conf.peer_stretch = conf_.network_max_peer_count / conf_.network_ideal_peer_count;
  taraxa_net_conf.is_boot_node = conf_.network_is_boot_node;
  taraxa_net_conf.expected_parallelism = tp_.capacity();

  string net_version = "TaraxaNode";  // TODO maybe give a proper name?
  auto construct_capabilities = [&, this](auto host) {
    assert(!host.expired());

    taraxa_capability_ = std::make_shared<network::tarcap::TaraxaCapability>(
        host, key, conf_, db, pbft_mgr, pbft_chain, vote_mgr, next_votes_mgr, dag_mgr, dag_blk_mgr, trx_mgr, key.address());
    return dev::p2p::Host::CapabilityList{taraxa_capability_};
  };
  host_ = dev::p2p::Host::make(net_version, construct_capabilities, key, net_conf, move(taraxa_net_conf),
                               network_file_path);

  for (uint i = 0; i < tp_.capacity(); ++i) {
    tp_.post_loop({100 + i * 20}, [this] {
      while (0 < host_->do_work())
        ;
    });
  }
}

Network::~Network() {
  tp_.stop();
  taraxa_capability_->stop();
}

void Network::start() {
  tp_.start();
  taraxa_capability_->start();
  LOG(log_nf_) << "Started Network address: " << conf_.network_address << ":" << conf_.network_tcp_port << std::endl;
  LOG(log_nf_) << "Started Node id: " << host_->id();
}

bool Network::isStarted() { return tp_.is_running(); }

std::list<dev::p2p::NodeEntry> Network::getAllNodes() const { return host_->getNodes(); }

size_t Network::getPeerCount() { return taraxa_capability_->getPeersState()->getPeersCount(); }

unsigned Network::getNodeCount() { return host_->getNodeCount(); }

Json::Value Network::getStatus() { return taraxa_capability_->getNodeStats()->getStatus(); }

std::vector<dev::p2p::NodeID> Network::getAllPeersIDs() const {
  return taraxa_capability_->getPeersState()->getAllPeersIDs();
}

void Network::onNewBlockVerified(shared_ptr<DagBlock> const &blk, bool proposed) {
  taraxa_capability_->onNewBlockVerified(blk, proposed);
  LOG(log_dg_) << "On new block verified:" << blk->getHash().toString();
}

void Network::onNewTransactions(std::vector<taraxa::bytes> transactions) {
  taraxa_capability_->onNewTransactions(transactions);
  LOG(log_dg_) << "On new transactions" << transactions.size();
}

void Network::restartSyncingPbft(bool force) {
  tp_.post([this, force] { taraxa_capability_->restartSyncingPbft(force); });
}

void Network::onNewPbftBlock(std::shared_ptr<PbftBlock> const &pbft_block) {
  LOG(log_dg_) << "Network broadcast PBFT block: " << pbft_block->getBlockHash();
  taraxa_capability_->onNewPbftBlock(pbft_block);
}

bool Network::pbft_syncing() { return taraxa_capability_->pbft_syncing(); }

uint64_t Network::syncTimeSeconds() const { return taraxa_capability_->getNodeStats()->syncTimeSeconds(); }

void Network::onNewPbftVotes(std::vector<Vote> votes) {
  for (auto const &vote : votes) {
    LOG(log_dg_) << "Network broadcast PBFT vote: " << vote.getHash();
    taraxa_capability_->onNewPbftVote(vote);
  }
}

void Network::broadcastPreviousRoundNextVotesBundle() {
  LOG(log_dg_) << "Network broadcast previous round next votes bundle";
  taraxa_capability_->broadcastPreviousRoundNextVotesBundle();
}

// METHODS USED IN TESTS ONLY
void Network::sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk) {
  taraxa_capability_->sendBlock(id, blk);
  LOG(log_dg_) << "Sent Block:" << blk.getHash().toString();
}

void Network::sendBlocks(dev::p2p::NodeID const &id, std::vector<std::shared_ptr<DagBlock>> blocks) {
  LOG(log_dg_) << "Sending Blocks:" << blocks.size();
  taraxa_capability_->sendBlocks(id, std::move(blocks));
}

void Network::sendTransactions(dev::p2p::NodeID const &_id, std::vector<taraxa::bytes> const &transactions) {
  taraxa_capability_->sendTransactions(_id, transactions);
  LOG(log_dg_) << "Sent transactions:" << transactions.size();
}

// TODO remove
void Network::setPendingPeersToReady() {
  const auto &peers_state = taraxa_capability_->getPeersState();

  auto peerIds = peers_state->getAllPendingPeersIDs();
  for (const auto &peerId : peerIds) {
    auto peer = peers_state->getPendingPeer(peerId);
    if (peer) {
      peers_state->setPeerAsReadyToSendMessages(peerId, peer);
    }
  }
}

dev::p2p::NodeID Network::getNodeId() const { return host_->id(); }

int Network::getReceivedBlocksCount() const { return taraxa_capability_->getReceivedBlocksCount(); }

int Network::getReceivedTransactionsCount() const { return taraxa_capability_->getReceivedTransactionsCount(); }

std::shared_ptr<network::tarcap::TaraxaPeer> Network::getPeer(dev::p2p::NodeID const &id) const {
  return taraxa_capability_->getPeersState()->getPeer(id);
}

void Network::sendPbftBlock(dev::p2p::NodeID const &id, PbftBlock const &pbft_block, uint64_t const &pbft_chain_size) {
  LOG(log_dg_) << "Network send PBFT block: " << pbft_block.getBlockHash() << " to: " << id;
  taraxa_capability_->sendPbftBlock(id, pbft_block, pbft_chain_size);
}

void Network::sendPbftVote(dev::p2p::NodeID const &id, Vote const &vote) {
  LOG(log_dg_) << "Network sent PBFT vote: " << vote.getHash() << " to: " << id;
  taraxa_capability_->sendPbftVote(id, vote);
}

}  // namespace taraxa
