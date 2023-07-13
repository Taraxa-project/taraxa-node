#include "network/tarcap/packets_handlers/latest/status_packet_handler.hpp"

#include "config/version.hpp"
#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/latest/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

StatusPacketHandler::StatusPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                         std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                         std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                         std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                         h256 genesis_hash, const addr_t& node_addr, const std::string& logs_prefix)
    : ExtSyncingPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                              logs_prefix + "STATUS_PH"),
      kGenesisHash(genesis_hash) {}

void StatusPacketHandler::validatePacketRlpFormat(const threadpool::PacketData& packet_data) const {
  if (const auto items_count = packet_data.rlp_.itemCount();
      items_count != kInitialStatusPacketItemsCount && items_count != kStandardStatusPacketItemsCount) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(),
                                        kStandardStatusPacketItemsCount);
  }
}

void StatusPacketHandler::process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  // Important !!! Use only "selected_peer" and not "peer" in this function as "peer" might be nullptr
  auto selected_peer = peer;
  const auto pbft_synced_period = pbft_mgr_->pbftSyncingPeriod();

  // Initial status packet
  if (packet_data.rlp_.itemCount() == kInitialStatusPacketItemsCount) {
    if (!selected_peer) {
      selected_peer = peers_state_->getPendingPeer(packet_data.from_node_id_);
      if (!selected_peer) {
        LOG(log_wr_) << "Peer " << packet_data.from_node_id_.abridged()
                     << " missing in both peers and pending peers map - will be disconnected.";
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    }

    auto it = packet_data.rlp_.begin();
    auto const peer_chain_id = (*it++).toInt<uint64_t>();
    auto const peer_dag_level = (*it++).toInt<uint64_t>();
    auto const genesis_hash = (*it++).toHash<blk_hash_t>();
    auto const peer_pbft_chain_size = (*it++).toInt<PbftPeriod>();
    auto const peer_syncing = (*it++).toInt();
    auto const peer_pbft_round = (*it++).toInt<PbftRound>();
    auto const node_major_version = (*it++).toInt<unsigned>();
    auto const node_minor_version = (*it++).toInt<unsigned>();
    auto const node_patch_version = (*it++).toInt<unsigned>();
    auto const is_light_node = (*it++).toInt();
    auto const node_history = (*it++).toInt<PbftPeriod>();

    if (peer_chain_id != kConf.genesis.chain_id) {
      LOG((peers_state_->getPeersCount()) ? log_nf_ : log_er_)
          << "Incorrect network id " << peer_chain_id << ", host " << packet_data.from_node_id_.abridged()
          << " will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (genesis_hash != kGenesisHash) {
      LOG((peers_state_->getPeersCount()) ? log_nf_ : log_er_)
          << "Incorrect genesis hash " << genesis_hash << ", host " << packet_data.from_node_id_.abridged()
          << " will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    // If this is a light node and it cannot serve our sync request disconnect from it
    if (is_light_node) {
      selected_peer->peer_light_node = true;
      selected_peer->peer_light_node_history = node_history;
      if (pbft_synced_period + node_history < peer_pbft_chain_size) {
        LOG((peers_state_->getPeersCount()) ? log_nf_ : log_er_)
            << "Light node " << packet_data.from_node_id_.abridged()
            << " would not be able to serve our syncing request. "
            << "Current synced period " << pbft_synced_period << ", peer synced period " << pbft_synced_period
            << ", peer light node history " << node_history << ". Peer will be disconnected";
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    }

    selected_peer->dag_level_ = peer_dag_level;
    selected_peer->pbft_chain_size_ = peer_pbft_chain_size;
    selected_peer->syncing_ = peer_syncing;
    selected_peer->pbft_period_ = peer_pbft_chain_size + 1;
    selected_peer->pbft_round_ = peer_pbft_round;

    peers_state_->setPeerAsReadyToSendMessages(packet_data.from_node_id_, selected_peer);

    LOG(log_dg_) << "Received initial status message from " << packet_data.from_node_id_ << ", network id "
                 << peer_chain_id << ", peer DAG max level " << selected_peer->dag_level_ << ", genesis "
                 << genesis_hash << ", peer pbft chain size " << selected_peer->pbft_chain_size_ << ", peer syncing "
                 << std::boolalpha << selected_peer->syncing_ << ", peer pbft period " << selected_peer->pbft_period_
                 << ", peer pbft round " << selected_peer->pbft_round_ << ", node major version" << node_major_version
                 << ", node minor version" << node_minor_version << ", node patch version" << node_patch_version;

  } else {  // Standard status packet
    // TODO: initial and standard status packet could be separated...
    if (!selected_peer) {
      LOG(log_er_) << "Received standard status packet from " << packet_data.from_node_id_.abridged()
                   << ", without previously received initial status packet. Will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    auto it = packet_data.rlp_.begin();
    selected_peer->dag_level_ = (*it++).toInt<uint64_t>();
    selected_peer->pbft_chain_size_ = (*it++).toInt<PbftPeriod>();
    selected_peer->pbft_period_ = selected_peer->pbft_chain_size_ + 1;
    selected_peer->syncing_ = (*it++).toInt();
    selected_peer->pbft_round_ = (*it++).toInt<PbftRound>();

    // TODO: Address malicious status
    if (!pbft_syncing_state_->isPbftSyncing()) {
      if (pbft_synced_period < selected_peer->pbft_chain_size_) {
        LOG(log_nf_) << "Restart PBFT chain syncing. Own synced PBFT at period " << pbft_synced_period
                     << ", peer PBFT chain size " << selected_peer->pbft_chain_size_;
        if (pbft_synced_period + 1 < selected_peer->pbft_chain_size_) {
          startSyncingPbft();
        } else {
          // If we are behind by only one block wait for two status messages before syncing because nodes are not always
          // in perfect sync
          if (selected_peer->last_status_pbft_chain_size_ == selected_peer->pbft_chain_size_) {
            startSyncingPbft();
          }
        }
      } else if (pbft_synced_period == selected_peer->pbft_chain_size_ && !selected_peer->peer_dag_synced_) {
        // if not syncing and the peer period is matching our period request any pending dag blocks
        requestPendingDagBlocks(selected_peer);
      }

      const auto [pbft_current_round, pbft_current_period] = pbft_mgr_->getPbftRoundAndPeriod();
      if (pbft_current_period == selected_peer->pbft_period_ && pbft_current_round < selected_peer->pbft_round_) {
        requestPbftNextVotesAtPeriodRound(selected_peer->getId(), pbft_current_period, pbft_current_round);
      }
    }
    selected_peer->last_status_pbft_chain_size_ = selected_peer->pbft_chain_size_.load();

    LOG(log_dg_) << "Received status message from " << packet_data.from_node_id_ << ", peer DAG max level "
                 << selected_peer->dag_level_ << ", peer pbft chain size " << selected_peer->pbft_chain_size_
                 << ", peer syncing " << std::boolalpha << selected_peer->syncing_ << ", peer pbft round "
                 << selected_peer->pbft_round_;
  }
}

bool StatusPacketHandler::sendStatus(const dev::p2p::NodeID& node_id, bool initial) {
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
        node_id, StatusPacket,
        std::move(dev::RLPStream(kInitialStatusPacketItemsCount)
                  << kConf.genesis.chain_id << dag_max_level << kGenesisHash << pbft_chain_size
                  << pbft_syncing_state_->isPbftSyncing() << pbft_round << TARAXA_MAJOR_VERSION << TARAXA_MINOR_VERSION
                  << TARAXA_PATCH_VERSION << dag_mgr_->isLightNode() << dag_mgr_->getLightNodeHistory()));
  } else {
    success = sealAndSend(
        node_id, StatusPacket,
        std::move(dev::RLPStream(kStandardStatusPacketItemsCount)
                  << dag_max_level << pbft_chain_size << pbft_syncing_state_->isDeepPbftSyncing() << pbft_round));
  }

  return success;
}

void StatusPacketHandler::sendStatusToPeers() {
  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "Unavailable host during checkLiveness";
    return;
  }

  for (auto const& peer : peers_state_->getAllPeers()) {
    sendStatus(peer.first, false);
  }
}

}  // namespace taraxa::network::tarcap
