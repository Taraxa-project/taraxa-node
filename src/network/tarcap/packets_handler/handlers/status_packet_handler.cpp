#include "status_packet_handler.hpp"

#include "config/version.hpp"
#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "dag/dag.hpp"
#include "network/tarcap/packets_handler/handlers/common/syncing_handler.hpp"
#include "network/tarcap/packets_handler/syncing_state.hpp"

namespace taraxa::network::tarcap {

StatusPacketHandler::StatusPacketHandler(std::shared_ptr<PeersState> peers_state,
                                         std::shared_ptr<PacketsStats> packets_stats,
                                         std::shared_ptr<SyncingState> syncing_state,
                                         std::shared_ptr<SyncingHandler> syncing_handler,
                                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DagManager> dag_mgr,
                                         uint64_t conf_network_id, const addr_t& node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "Status_PH"),
      syncing_state_(std::move(syncing_state)),
      syncing_handler_(std::move(syncing_handler)),
      pbft_chain_(std::move(pbft_chain)),
      dag_mgr_(std::move(dag_mgr)),
      conf_network_id_(conf_network_id) {}

void StatusPacketHandler::process(const PacketData& packet_data, const dev::RLP& packet_rlp) {
  bool initial_status = packet_rlp.itemCount() == INITIAL_STATUS_PACKET_ITEM_COUNT;

  if (initial_status) {
    auto it = packet_rlp.begin();
    auto const peer_network_id = (*it++).toInt<uint64_t>();
    auto const peer_dag_level = (*it++).toPositiveInt64();
    auto const genesis_hash = blk_hash_t(*it++);
    auto const peer_pbft_chain_size = (*it++).toPositiveInt64();
    auto const peer_syncing = (*it++).toInt();
    auto const peer_pbft_round = (*it++).toPositiveInt64();
    auto const peer_pbft_previous_round_next_votes_size = (*it++).toInt<unsigned>();
    auto const node_major_version = (*it++).toInt<unsigned>();
    auto const node_minor_version = (*it++).toInt<unsigned>();
    auto const node_patch_version = (*it++).toInt<unsigned>();

    // We need logic when some different node versions might still be compatible
    if (node_major_version != TARAXA_MAJOR_VERSION || node_minor_version != TARAXA_MINOR_VERSION) {
      LOG(log_er_) << "Incorrect node version: "
                   << getFormattedVersion({node_major_version, node_minor_version, node_patch_version})
                   << ", our node version" << TARAXA_VERSION << ", host " << packet_data.from_node_id_.abridged()
                   << " will be disconnected";
      // TODO: get rid of host_ weak_ptr if possible ???
      tmp_host_->disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (peer_network_id != conf_network_id_) {
      LOG(log_er_) << "Incorrect network id " << peer_network_id << ", host " << packet_data.from_node_id_.abridged()
                   << " will be disconnected";
      tmp_host_->disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    if (genesis_hash != dag_mgr_->get_genesis()) {
      LOG(log_er_) << "Incorrect genesis hash " << genesis_hash << ", host " << packet_data.from_node_id_.abridged()
                   << " will be disconnected";
      tmp_host_->disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
      return;
    }

    tmp_peer_->dag_level_ = peer_dag_level;
    tmp_peer_->pbft_chain_size_ = peer_pbft_chain_size;
    tmp_peer_->syncing_ = peer_syncing;
    tmp_peer_->pbft_round_ = peer_pbft_round;
    tmp_peer_->pbft_previous_round_next_votes_size_ = peer_pbft_previous_round_next_votes_size;

    peers_state_->setPeerAsReadyToSendMessages(packet_data.from_node_id_, tmp_peer_);

    LOG(log_dg_) << "Received initial status message from " << packet_data.from_node_id_ << ", network id "
                 << peer_network_id << ", peer DAG max level " << tmp_peer_->dag_level_ << ", genesis " << genesis_hash
                 << ", peer pbft chain size " << tmp_peer_->pbft_chain_size_ << ", peer syncing " << std::boolalpha
                 << tmp_peer_->syncing_ << ", peer pbft round " << tmp_peer_->pbft_round_
                 << ", peer pbft previous round next votes size " << tmp_peer_->pbft_previous_round_next_votes_size_
                 << ", node major version" << node_major_version << ", node minor version" << node_minor_version;

  } else {
    auto it = packet_rlp.begin();
    tmp_peer_->dag_level_ = (*it++).toPositiveInt64();
    tmp_peer_->pbft_chain_size_ = (*it++).toPositiveInt64();
    tmp_peer_->syncing_ = (*it++).toInt();
    tmp_peer_->pbft_round_ = (*it++).toPositiveInt64();
    tmp_peer_->pbft_previous_round_next_votes_size_ = (*it++).toInt<unsigned>();

    LOG(log_dg_) << "Received status message from " << packet_data.from_node_id_ << ", peer DAG max level "
                 << tmp_peer_->dag_level_ << ", peer pbft chain size " << tmp_peer_->pbft_chain_size_
                 << ", peer syncing " << std::boolalpha << tmp_peer_->syncing_ << ", peer pbft round "
                 << tmp_peer_->pbft_round_ << ", peer pbft previous round next votes size "
                 << tmp_peer_->pbft_previous_round_next_votes_size_;
  }

  // TODO: do we keep syncing specific channels ?
  /*
      LOG(log_dg_dag_sync_) << "Received status message from " << packet_data.from_node_id_ << " peer DAG max level:" <<
      peer_->dag_level_; LOG(log_dg_pbft_sync_) << "Received status message from " << packet_data.from_node_id_ << ",
      peer sycning: " << std::boolalpha
                             << peer_->syncing_ << ", peer PBFT chain size:" << peer_->pbft_chain_size_;
      LOG(log_dg_next_votes_sync_) << "Received status message from " << packet_data.from_node_id_ << ", PBFT round " <<
      peer_->pbft_round_
                                   << ", peer PBFT previous round next votes size "
                                   << peer_->pbft_previous_round_next_votes_size_;
  */
  // If we are still syncing - do not trigger new syncing
  if (syncing_state_->is_pbft_syncing() && syncing_state_->is_actively_syncing()) {
    LOG(log_dg_) << "There is ongoing syncing, do not trigger new one.";
    return;
  }

  // TODO: Address the CONCERN that it isn't NECESSARY to sync here
  // and by syncing here we open node up to attack of sending bogus
  // status.  We also have nothing to punish a node failing to send
  // sync info.
  auto pbft_synced_period = pbft_chain_->pbftSyncingPeriod();
  if (pbft_synced_period + 1 < tmp_peer_->pbft_chain_size_) {
    LOG(log_nf_) << "Restart PBFT chain syncing. Own synced PBFT at period " << pbft_synced_period
                 << ", peer PBFT chain size " << tmp_peer_->pbft_chain_size_;
    if (pbft_synced_period + 5 < tmp_peer_->pbft_chain_size_) {
      syncing_handler_->restartSyncingPbft(true);
    } else {
      syncing_handler_->restartSyncingPbft(false);
    }
  }

  auto pbft_current_round = pbft_mgr_->getPbftRound();
  auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesSize();
  if (pbft_current_round < tmp_peer_->pbft_round_ ||
      (pbft_current_round == tmp_peer_->pbft_round_ &&
       pbft_previous_round_next_votes_size < tmp_peer_->pbft_previous_round_next_votes_size_)) {
    syncing_handler_->syncPbftNextVotes(pbft_current_round, pbft_previous_round_next_votes_size);
  }
}

bool StatusPacketHandler::sendStatus(const dev::p2p::NodeID& node_id, bool initial) {
  bool success = false;

  if (dag_mgr_) {
    std::string status_packet_type = initial ? "initial" : "standard";

    LOG(log_dg_) << "Sending initial " << status_packet_type << " status message to " << node_id
                 << ", protocol version " << TARAXA_NET_VERSION << ", network id " << conf_network_id_ << ", genesis "
                 << dag_mgr_->get_genesis() << ", node version " << TARAXA_VERSION;

    auto dag_max_level = dag_mgr_->getMaxLevel();
    auto pbft_chain_size = pbft_chain_->getPbftChainSize();
    auto pbft_round = pbft_mgr_->getPbftRound();
    auto pbft_previous_round_next_votes_size = next_votes_mgr_->getNextVotesSize();

    // TODO: do we keep syncing specific channels ?
    /*
        LOG(log_dg_dag_sync_) << "Sending status message to " << node_id << " with dag level: " << dag_max_level;
        LOG(log_dg_pbft_sync_) << "Sending status message to " << node_id << " with pbft chain size: " <<
        pbft_chain_size
                               << ", syncing: " << std::boolalpha << syncing_state_->is_pbft_syncing();
        LOG(log_dg_next_votes_sync_) << "Sending status message to " << node_id << " with PBFT round: " << pbft_round
                                     << ", previous round next votes size " << pbft_previous_round_next_votes_size;
    */

    if (initial) {
      success =
          sealAndSend(node_id, StatusPacket,
                      dev::RLPStream(INITIAL_STATUS_PACKET_ITEM_COUNT)
                          << conf_network_id_ << dag_max_level << dag_mgr_->get_genesis() << pbft_chain_size
                          << syncing_state_->is_pbft_syncing() << pbft_round << pbft_previous_round_next_votes_size
                          << TARAXA_MAJOR_VERSION << TARAXA_MINOR_VERSION << TARAXA_PATCH_VERSION);
    } else {
      success = sealAndSend(node_id, StatusPacket,
                            dev::RLPStream(5) << dag_max_level << pbft_chain_size << syncing_state_->is_pbft_syncing()
                                              << pbft_round << pbft_previous_round_next_votes_size);
    }
  }

  return success;
}

void StatusPacketHandler::checkLiveness() {
  auto host = peers_state_->host_.lock();
  if (!host) {
    LOG(log_er_) << "Unavailable host during checkLiveness";
    return;
  }

  for (auto const& peer : peers_state_->getAllPeers()) {
    // Disconnect any node that did not send any message for 3 status intervals
    if (!peer.second->isAlive(MAX_CHECK_ALIVE_COUNT)) {
      host->disconnect(peer.first, p2p::PingTimeout);
      LOG(log_nf_) << "Host " << peer.first << " disconnected, no status message received in " << MAX_CHECK_ALIVE_COUNT
                   << " check alive intervals";
    }

    // Send status message
    else {
      sendStatus(peer.first, false);
    }
  }
}

}  // namespace taraxa::network::tarcap