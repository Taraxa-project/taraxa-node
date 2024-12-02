#include "network/tarcap/packets_handlers/interface/sync_packet_handler.hpp"

#include "config/version.hpp"
#include "network/tarcap/packets/latest/get_pbft_sync_packet.hpp"
#include "network/tarcap/packets/latest/status_packet.hpp"

namespace taraxa::network::tarcap {

ISyncPacketHandler::ISyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                       std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                       std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                       std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                       std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                       const addr_t& node_addr, const std::string& logs_prefix)
    : ExtSyncingPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                              logs_prefix + "STATUS_PH"),
      kGenesisHash(kConf.genesis.genesisHash()) {}

void ISyncPacketHandler::startSyncingPbft() {
  if (pbft_syncing_state_->isPbftSyncing()) {
    LOG(this->log_dg_) << "startSyncingPbft called but syncing_ already true";
    return;
  }

  std::shared_ptr<TaraxaPeer> peer = peers_state_->getMaxChainPeer(pbft_mgr_);
  if (!peer) {
    LOG(this->log_nf_) << "Restarting syncing PBFT not possible since no connected peers";
    return;
  }

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (peer->pbft_chain_size_ > pbft_sync_period) {
    auto peer_id = peer->getId().abridged();
    auto peer_pbft_chain_size = peer->pbft_chain_size_.load();
    if (!pbft_syncing_state_->setPbftSyncing(true, pbft_sync_period, std::move(peer))) {
      LOG(this->log_dg_) << "startSyncingPbft called but syncing_ already true";
      return;
    }
    LOG(this->log_si_) << "Restarting syncing PBFT from peer " << peer_id << ", peer PBFT chain size "
                       << peer_pbft_chain_size << ", own PBFT chain synced at period " << pbft_sync_period;

    if (syncPeerPbft(pbft_sync_period + 1)) {
      // Disable snapshots only if are syncing from scratch
      if (pbft_syncing_state_->isDeepPbftSyncing()) {
        db_->disableSnapshots();
      }
    } else {
      pbft_syncing_state_->setPbftSyncing(false);
    }
  } else {
    LOG(this->log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                       << pbft_chain_->getPbftChainSize() << ")"
                       << " is greater or equal than max node pbft chain size:" << peer->pbft_chain_size_;
    db_->enableSnapshots();
  }
}

bool ISyncPacketHandler::syncPeerPbft(PbftPeriod request_period) {
  const auto syncing_peer = pbft_syncing_state_->syncingPeer();
  if (!syncing_peer) {
    LOG(this->log_er_) << "Unable to send GetPbftSyncPacket. No syncing peer set.";
    return false;
  }

  if (request_period > syncing_peer->pbft_chain_size_) {
    LOG(this->log_wr_) << "Invalid syncPeerPbft argument. Node " << syncing_peer->getId() << " chain size "
                       << syncing_peer->pbft_chain_size_ << ", requested period " << request_period;
    return false;
  }

  LOG(this->log_nf_) << "Send GetPbftSyncPacket with period " << request_period << " to node " << syncing_peer->getId();
  return this->sealAndSend(syncing_peer->getId(), SubprotocolPacketType::kGetPbftSyncPacket,
                           encodePacketRlp(GetPbftSyncPacket{request_period}));
}

void ISyncPacketHandler::sendStatusToPeers() {
  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "Unavailable host during checkLiveness";
    return;
  }

  for (auto const& peer : peers_state_->getAllPeers()) {
    sendStatus(peer.first, false);
  }
}

bool ISyncPacketHandler::sendStatus(const dev::p2p::NodeID& node_id, bool initial) {
  bool success = false;
  std::string status_packet_type = initial ? "initial" : "standard";

  LOG(log_dg_) << "Sending " << status_packet_type << " status message to " << node_id << ", protocol version "
               << TARAXA_NET_VERSION << ", network id " << kConf.genesis.chain_id << ", genesis " << kGenesisHash
               << ", node version " << TARAXA_VERSION;

  auto dag_max_level = dag_mgr_->getMaxLevel();
  auto pbft_chain_size = pbft_chain_->getPbftChainSize();
  const auto pbft_round = pbft_mgr_->getPbftRound();

  if (initial) {
    success = sealAndSend(
        node_id, SubprotocolPacketType::kStatusPacket,
        encodePacketRlp(StatusPacket(
            pbft_chain_size, pbft_round, dag_max_level, pbft_syncing_state_->isPbftSyncing(),
            StatusPacket::InitialData{kConf.genesis.chain_id, kGenesisHash, TARAXA_MAJOR_VERSION, TARAXA_MINOR_VERSION,
                                      TARAXA_PATCH_VERSION, kConf.is_light_node, kConf.light_node_history})));
  } else {
    success = sealAndSend(node_id, SubprotocolPacketType::kStatusPacket,
                          encodePacketRlp(StatusPacket(pbft_chain_size, pbft_round, dag_max_level,
                                                       pbft_syncing_state_->isDeepPbftSyncing())));
  }

  return success;
}

}  // namespace taraxa::network::tarcap
