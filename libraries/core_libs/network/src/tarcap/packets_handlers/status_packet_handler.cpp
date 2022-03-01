#include "network/tarcap/packets_handlers/status_packet_handler.hpp"

#include "config/version.hpp"
#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

StatusPacketHandler::StatusPacketHandler(
    std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
    std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
    std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
    std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<NextVotesManager> next_votes_mgr,
    std::shared_ptr<DbStorage> db, uint64_t conf_network_id, const addr_t& node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(dag_blk_mgr),
                              std::move(db), node_addr, "STATUS_PH"),
      conf_network_id_(conf_network_id),
      next_votes_mgr_(std::move(next_votes_mgr)) {}

void StatusPacketHandler::validatePacketRlpFormat(const PacketData& packet_data) const {
  if (const auto items_count = packet_data.rlp_.itemCount();
      items_count != kInitialStatusPacketItemsCount && items_count != kStandardStatusPacketItemsCount) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(),
                                        kStandardStatusPacketItemsCount);
  }
}

void StatusPacketHandler::process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
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
    auto const peer_network_id = (*it++).toInt<uint64_t>();
    auto const peer_dag_level = (*it++).toInt<uint64_t>();
    auto const genesis_hash = (*it++).toHash<blk_hash_t>();
    auto const peer_pbft_chain_size = (*it++).toInt<uint64_t>();
    auto const peer_syncing = (*it++).toInt();
    auto const peer_pbft_round = (*it++).toInt<uint64_t>();
    auto const peer_pbft_previous_round_next_votes_size = (*it++).toInt<unsigned>();
    auto const node_major_version = (*it++).toInt<unsigned>();
    auto const node_minor_version = (*it++).toInt<unsigned>();
    auto const node_patch_version = (*it++).toInt<unsigned>();

    // We need logic when some different node versions might still be compatible
    if (node_major_version != TARAXA_MAJOR_VERSION || node_minor_version != TARAXA_MINOR_VERSION) {
      // Log this only if we have 0 peers, so it is error of this node
      LOG((peers_state_->getPeersCount()) ? log_nf_ : log_er_)
          << "Incorrect node version: "
          << getFormattedVersion({node_major_version, node_minor_version, node_patch_version}) << ", our node version"
          << TARAXA_VERSION << ", host " << packet_data.from_node_id_.abridged() << " will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (peer_network_id != conf_network_id_) {
      LOG((peers_state_->getPeersCount()) ? log_nf_ : log_er_)
          << "Incorrect network id " << peer_network_id << ", host " << packet_data.from_node_id_.abridged()
          << " will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (genesis_hash != dag_mgr_->get_genesis()) {
      LOG((peers_state_->getPeersCount()) ? log_nf_ : log_er_)
          << "Incorrect genesis hash " << genesis_hash << ", host " << packet_data.from_node_id_.abridged()
          << " will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    selected_peer->dag_level_ = peer_dag_level;
    selected_peer->pbft_chain_size_ = peer_pbft_chain_size;
    selected_peer->syncing_ = peer_syncing;
    selected_peer->pbft_round_ = peer_pbft_round;
    selected_peer->pbft_previous_round_next_votes_size_ = peer_pbft_previous_round_next_votes_size;

    peers_state_->setPeerAsReadyToSendMessages(packet_data.from_node_id_, selected_peer);

    LOG(log_dg_) << "Received initial status message from " << packet_data.from_node_id_ << ", network id "
                 << peer_network_id << ", peer DAG max level " << selected_peer->dag_level_ << ", genesis "
                 << genesis_hash << ", peer pbft chain size " << selected_peer->pbft_chain_size_ << ", peer syncing "
                 << std::boolalpha << selected_peer->syncing_ << ", peer pbft round " << selected_peer->pbft_round_
                 << ", peer pbft previous round next votes size " << selected_peer->pbft_previous_round_next_votes_size_
                 << ", node major version" << node_major_version << ", node minor version" << node_minor_version;

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
    selected_peer->pbft_chain_size_ = (*it++).toInt<uint64_t>();
    selected_peer->syncing_ = (*it++).toInt();
    selected_peer->pbft_round_ = (*it++).toInt<uint64_t>();
    selected_peer->pbft_previous_round_next_votes_size_ = (*it++).toInt<unsigned>();

    // TODO: Address malicious status
    if (!syncing_state_->is_pbft_syncing()) {
      if (pbft_synced_period < selected_peer->pbft_chain_size_) {
        LOG(log_nf_) << "Restart PBFT chain syncing. Own synced PBFT at period " << pbft_synced_period
                     << ", peer PBFT chain size " << selected_peer->pbft_chain_size_;
        if (pbft_synced_period + 1 < selected_peer->pbft_chain_size_) {
          restartSyncingPbft();
        } else {
          // If we are behind by only one block wait for two status messages before syncing because nodes are not always
          // in perfect sync
          if (selected_peer->last_status_pbft_chain_size_ == selected_peer->pbft_chain_size_) {
            restartSyncingPbft();
          }
        }
      } else if (pbft_synced_period == selected_peer->pbft_chain_size_ && !selected_peer->peer_dag_synced_) {
        // if not syncing and the peer period is matching our period request any pending dag blocks
        requestPendingDagBlocks(selected_peer);
      }
      const auto pbft_current_round = pbft_mgr_->getPbftRound();
      const auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesWeight();
      if (pbft_current_round < selected_peer->pbft_round_) {
        syncPbftNextVotes(pbft_current_round, pbft_previous_round_next_votes_size);
      } else if (pbft_current_round == selected_peer->pbft_round_) {
        const auto two_times_2t_plus_1 = pbft_mgr_->getTwoTPlusOne() * 2;
        // Node at lease have one next vote value for previoud PBFT round. There may have 2 next vote values for
        // previous PBFT round. If node own have one next vote value and peer have two, need sync here.
        if (pbft_previous_round_next_votes_size < two_times_2t_plus_1 &&
            selected_peer->pbft_previous_round_next_votes_size_ >= two_times_2t_plus_1) {
          syncPbftNextVotes(pbft_current_round, pbft_previous_round_next_votes_size);
        }
      }
    }
    selected_peer->last_status_pbft_chain_size_ = selected_peer->pbft_chain_size_.load();

    LOG(log_dg_) << "Received status message from " << packet_data.from_node_id_ << ", peer DAG max level "
                 << selected_peer->dag_level_ << ", peer pbft chain size " << selected_peer->pbft_chain_size_
                 << ", peer syncing " << std::boolalpha << selected_peer->syncing_ << ", peer pbft round "
                 << selected_peer->pbft_round_ << ", peer pbft previous round next votes size "
                 << selected_peer->pbft_previous_round_next_votes_size_;
  }
}

bool StatusPacketHandler::sendStatus(const dev::p2p::NodeID& node_id, bool initial) {
  bool success = false;

  if (dag_mgr_) {
    std::string status_packet_type = initial ? "initial" : "standard";

    LOG(log_dg_) << "Sending " << status_packet_type << " status message to " << node_id << ", protocol version "
                 << TARAXA_NET_VERSION << ", network id " << conf_network_id_ << ", genesis " << dag_mgr_->get_genesis()
                 << ", node version " << TARAXA_VERSION;

    auto dag_max_level = dag_mgr_->getMaxLevel();
    auto pbft_chain_size = pbft_chain_->getPbftChainSize();
    auto pbft_round = pbft_mgr_->getPbftRound();
    auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesWeight();

    if (initial) {
      success = sealAndSend(
          node_id, StatusPacket,
          std::move(dev::RLPStream(kInitialStatusPacketItemsCount)
                    << conf_network_id_ << dag_max_level << dag_mgr_->get_genesis() << pbft_chain_size
                    << syncing_state_->is_pbft_syncing() << pbft_round << pbft_previous_round_next_votes_size
                    << TARAXA_MAJOR_VERSION << TARAXA_MINOR_VERSION << TARAXA_PATCH_VERSION));
    } else {
      success = sealAndSend(node_id, StatusPacket,
                            std::move(dev::RLPStream(kStandardStatusPacketItemsCount)
                                      << dag_max_level << pbft_chain_size << syncing_state_->is_deep_pbft_syncing()
                                      << pbft_round << pbft_previous_round_next_votes_size));
    }
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

void StatusPacketHandler::syncPbftNextVotes(uint64_t pbft_round, size_t pbft_previous_round_next_votes_size) {
  dev::p2p::NodeID peer_node_ID;
  uint64_t peer_max_pbft_round = 1;
  size_t peer_max_previous_round_next_votes_size = 0;

  auto peers = peers_state_->getAllPeers();
  // Find max peer PBFT round
  for (auto const& peer : peers) {
    if (peer.second->pbft_round_ > peer_max_pbft_round) {
      peer_max_pbft_round = peer.second->pbft_round_;
      peer_node_ID = peer.first;
    }
  }

  if (pbft_round == peer_max_pbft_round) {
    // No peers ahead, find peer PBFT previous round max next votes size
    for (auto const& peer : peers) {
      if (peer.second->pbft_previous_round_next_votes_size_ > peer_max_previous_round_next_votes_size) {
        peer_max_previous_round_next_votes_size = peer.second->pbft_previous_round_next_votes_size_;
        peer_node_ID = peer.first;
      }
    }
  }

  if (pbft_round < peer_max_pbft_round ||
      (pbft_round == peer_max_pbft_round &&
       pbft_previous_round_next_votes_size < peer_max_previous_round_next_votes_size)) {
    LOG(log_dg_) << "Syncing PBFT next votes. Current PBFT round " << pbft_round << ", previous round next votes size "
                 << pbft_previous_round_next_votes_size << ". Peer " << peer_node_ID << " is in PBFT round "
                 << peer_max_pbft_round << ", previous round next votes size "
                 << peer_max_previous_round_next_votes_size;
    requestPbftNextVotes(peer_node_ID, pbft_round, pbft_previous_round_next_votes_size);
  }
}

void StatusPacketHandler::requestPbftNextVotes(dev::p2p::NodeID const& peerID, uint64_t pbft_round,
                                               size_t pbft_previous_round_next_votes_size) {
  LOG(log_dg_) << "Sending GetVotesSyncPacket with round " << pbft_round << " previous round next votes size "
               << pbft_previous_round_next_votes_size;
  sealAndSend(peerID, GetVotesSyncPacket,
              std::move(dev::RLPStream(2) << pbft_round << pbft_previous_round_next_votes_size));
}

}  // namespace taraxa::network::tarcap
