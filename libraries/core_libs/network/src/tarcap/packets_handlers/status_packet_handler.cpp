#include "network/tarcap/packets_handlers/status_packet_handler.hpp"

#include "config/version.hpp"
#include "dag/dag.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

StatusPacketHandler::StatusPacketHandler(std::shared_ptr<PeersState> peers_state,
                                         std::shared_ptr<PacketsStats> packets_stats,
                                         std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                         std::shared_ptr<DagManager> dag_mgr,
                                         std::shared_ptr<NextVotesManager> next_votes_mgr,
                                         std::shared_ptr<DbStorage> db, uint64_t conf_chain_id, h256 genesis_hash,
                                         const addr_t& node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(pbft_syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(db), node_addr,
                              "STATUS_PH"),
      conf_chain_id_(conf_chain_id),
      genesis_hash_(genesis_hash),
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
    auto const peer_chain_id = (*it++).toInt<uint64_t>();
    auto const peer_dag_level = (*it++).toInt<uint64_t>();
    auto const genesis_hash = (*it++).toHash<blk_hash_t>();
    auto const peer_pbft_chain_size = (*it++).toInt<uint64_t>();
    auto const peer_syncing = (*it++).toInt();
    auto const peer_pbft_round = (*it++).toInt<uint64_t>();
    auto const peer_pbft_previous_round_next_votes_size = (*it++).toInt<unsigned>();
    auto const node_major_version = (*it++).toInt<unsigned>();
    auto const node_minor_version = (*it++).toInt<unsigned>();
    auto const node_patch_version = (*it++).toInt<unsigned>();
    auto const is_light_node = (*it++).toInt();
    auto const node_history = (*it++).toInt<uint64_t>();

    if (peer_chain_id != conf_chain_id_) {
      LOG((peers_state_->getPeersCount()) ? log_nf_ : log_er_)
          << "Incorrect network id " << peer_chain_id << ", host " << packet_data.from_node_id_.abridged()
          << " will be disconnected";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (genesis_hash != genesis_hash_) {
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
            << "Light node is not able to serve our syncing request. " << packet_data.from_node_id_.abridged()
            << " peer will be disconnected";
        disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
        return;
      }
    }

    selected_peer->dag_level_ = peer_dag_level;
    selected_peer->pbft_chain_size_ = peer_pbft_chain_size;
    selected_peer->syncing_ = peer_syncing;
    selected_peer->pbft_period_ = peer_pbft_chain_size + 1;
    selected_peer->pbft_round_ = peer_pbft_round;
    selected_peer->pbft_previous_round_next_votes_size_ = peer_pbft_previous_round_next_votes_size;

    peers_state_->setPeerAsReadyToSendMessages(packet_data.from_node_id_, selected_peer);

    LOG(log_dg_) << "Received initial status message from " << packet_data.from_node_id_ << ", network id "
                 << peer_chain_id << ", peer DAG max level " << selected_peer->dag_level_ << ", genesis "
                 << genesis_hash << ", peer pbft chain size " << selected_peer->pbft_chain_size_ << ", peer syncing "
                 << std::boolalpha << selected_peer->syncing_ << ", peer pbft period " << selected_peer->pbft_period_
                 << ", peer pbft round " << selected_peer->pbft_round_ << ", peer pbft previous round next votes size "
                 << selected_peer->pbft_previous_round_next_votes_size_ << ", node major version" << node_major_version
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
    selected_peer->pbft_chain_size_ = (*it++).toInt<uint64_t>();
    selected_peer->pbft_period_ = selected_peer->pbft_chain_size_ + 1;
    selected_peer->syncing_ = (*it++).toInt();
    selected_peer->pbft_round_ = (*it++).toInt<uint64_t>();
    selected_peer->pbft_previous_round_next_votes_size_ = (*it++).toInt<unsigned>();

    // TODO: Address malicious status
    if (!pbft_syncing_state_->isPbftSyncing()) {
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
      const auto [pbft_current_round, pbft_current_period] = pbft_mgr_->getPbftRoundAndPeriod();
      const auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesWeight();
      if (pbft_current_period == selected_peer->pbft_period_) {
        if (pbft_current_round < selected_peer->pbft_round_) {
          syncPbftNextVotesAtPeriodRound(pbft_current_period, pbft_current_round, pbft_previous_round_next_votes_size);
        } else if (pbft_current_round == selected_peer->pbft_round_) {
          if (const auto two_t_plus_one = pbft_mgr_->getPbftTwoTPlusOne(pbft_current_period - 1);
              two_t_plus_one.has_value()) {
            const auto two_times_2t_plus_1 = (*two_t_plus_one) * 2;
            // Node at least have one next vote value for previous PBFT round. There may have 2 next vote values for
            // previous PBFT round. If node own have one next vote value and peer have two, need sync here.
            if (pbft_previous_round_next_votes_size < two_times_2t_plus_1 &&
                selected_peer->pbft_previous_round_next_votes_size_ >= two_times_2t_plus_1) {
              syncPbftNextVotesAtPeriodRound(pbft_current_period, pbft_current_round,
                                             pbft_previous_round_next_votes_size);
            }
          }
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
                 << TARAXA_NET_VERSION << ", network id " << conf_chain_id_ << ", genesis " << genesis_hash_
                 << ", node version " << TARAXA_VERSION;

    auto dag_max_level = dag_mgr_->getMaxLevel();
    auto pbft_chain_size = pbft_chain_->getPbftChainSize();
    const auto pbft_round = pbft_mgr_->getPbftRound();
    auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesWeight();

    if (initial) {
      success =
          sealAndSend(node_id, StatusPacket,
                      std::move(dev::RLPStream(kInitialStatusPacketItemsCount)
                                << conf_chain_id_ << dag_max_level << genesis_hash_ << pbft_chain_size
                                << pbft_syncing_state_->isPbftSyncing() << pbft_round
                                << pbft_previous_round_next_votes_size << TARAXA_MAJOR_VERSION << TARAXA_MINOR_VERSION
                                << TARAXA_PATCH_VERSION << dag_mgr_->isLightNode() << dag_mgr_->getLightNodeHistory()));
    } else {
      success = sealAndSend(node_id, StatusPacket,
                            std::move(dev::RLPStream(kStandardStatusPacketItemsCount)
                                      << dag_max_level << pbft_chain_size << pbft_syncing_state_->isDeepPbftSyncing()
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

void StatusPacketHandler::syncPbftNextVotesAtPeriodRound(uint64_t pbft_period, uint64_t pbft_round,
                                                         size_t pbft_previous_round_next_votes_size) {
  dev::p2p::NodeID peer_node_ID;
  uint64_t peer_max_pbft_period = 1;
  uint64_t peer_max_pbft_round = 1;
  size_t peer_max_previous_round_next_votes_size = 0;

  auto peers = peers_state_->getAllPeers();
  // Find max peer PBFT period, round...
  for (auto const& peer : peers) {
    if ((peer.second->pbft_period_ > peer_max_pbft_period) ||
        (peer.second->pbft_period_ == peer_max_pbft_period && peer.second->pbft_round_ > peer_max_pbft_round)) {
      peer_max_pbft_period = peer.second->pbft_period_;
      peer_max_pbft_round = peer.second->pbft_round_;
      peer_node_ID = peer.first;
    }
  }

  if (pbft_period == peer_max_pbft_period && pbft_round == peer_max_pbft_round) {
    // No peers ahead, find peer PBFT previous round max next votes size
    for (auto const& peer : peers) {
      if (peer.second->pbft_previous_round_next_votes_size_ > peer_max_previous_round_next_votes_size) {
        peer_max_previous_round_next_votes_size = peer.second->pbft_previous_round_next_votes_size_;
        peer_node_ID = peer.first;
      }
    }
  }

  if (peer_max_pbft_period != pbft_period) {
    LOG(log_dg_) << "Syncing PBFT next votes not needed. Current PBFT period " << pbft_period
                 << ". Peer with max period: " << peer_node_ID << ", is in PBFT period: " << peer_max_pbft_period;
  } else if (pbft_round < peer_max_pbft_round ||
             (pbft_round == peer_max_pbft_round &&
              pbft_previous_round_next_votes_size < peer_max_previous_round_next_votes_size)) {
    LOG(log_dg_) << "Syncing PBFT next votes. In period " << pbft_period << ", current PBFT round " << pbft_round
                 << ", previous round next votes size " << pbft_previous_round_next_votes_size << ". Peer "
                 << peer_node_ID << " is in PBFT round " << peer_max_pbft_round << ", previous round next votes size "
                 << peer_max_previous_round_next_votes_size;
    requestPbftNextVotesAtPeriodRound(peer_node_ID, pbft_period, pbft_round, pbft_previous_round_next_votes_size);
  }
}

}  // namespace taraxa::network::tarcap
