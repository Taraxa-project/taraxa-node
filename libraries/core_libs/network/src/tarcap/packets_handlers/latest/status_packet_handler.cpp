#include "network/tarcap/packets_handlers/latest/status_packet_handler.hpp"

#include "config/version.hpp"
#include "network/tarcap/packets/latest/get_next_votes_bundle_packet.hpp"
#include "network/tarcap/packets/latest/status_packet.hpp"
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
                                         h256 genesis_hash, const std::string& logs_prefix)
    : ISyncPacketHandler(conf, peers_state, packets_stats, std::move(pbft_syncing_state), std::move(pbft_chain),
                         std::move(pbft_mgr), std::move(dag_mgr), std::move(db), logs_prefix + "STATUS_PH"),
      kGenesisHash(genesis_hash) {}

void StatusPacketHandler::process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<StatusPacket>(packet_data.rlp_);

  // Important !!! Use only "selected_peer" and not "peer" in this function as "peer" might be nullptr
  auto selected_peer = peer;
  const auto pbft_synced_period = pbft_mgr_->pbftSyncingPeriod();

  // Initial status packet
  if (packet.initial_data.has_value()) {
    if (!selected_peer) {
      selected_peer = peers_state_->getPendingPeer(peer->getId());
      if (!selected_peer) {
        logger_->warn("Peer {} missing in both peers and pending peers map - will be disconnected.",
                      peer->getId().abridged());
        disconnect(peer->getId(), dev::p2p::UserReason);
        return;
      }
    }

    if (packet.initial_data->peer_chain_id != kConf.genesis.chain_id) {
      if (peers_state_->getPeersCount()) {
        logger_->debug("Incorrect network id {}, host {} will be disconnected", packet.initial_data->peer_chain_id,
                       peer->getId().abridged());
      } else {
        logger_->error("Incorrect network id {}, host {} will be disconnected", packet.initial_data->peer_chain_id,
                       peer->getId().abridged());
      }
      disconnect(peer->getId(), dev::p2p::UserReason);
      return;
    }

    if (packet.initial_data->genesis_hash != kGenesisHash) {
      if (peers_state_->getPeersCount()) {
        logger_->info("Incorrect genesis hash {}, host {} will be disconnected", packet.initial_data->genesis_hash,
                      peer->getId().abridged());
      } else {
        logger_->warn("Incorrect genesis hash {}, host {} will be disconnected", packet.initial_data->genesis_hash,
                      peer->getId().abridged());
      }
      disconnect(peer->getId(), dev::p2p::UserReason);
      return;
    }

    // If this is a light node and it cannot serve our sync request disconnect from it
    if (packet.initial_data->is_light_node) {
      selected_peer->peer_light_node = true;
      selected_peer->peer_light_node_history = packet.initial_data->node_history;
      if (pbft_synced_period + packet.initial_data->node_history < packet.peer_pbft_chain_size) {
        if (peers_state_->getPeersCount()) {
          logger_->debug(
              "Light node {} would not be able to serve our syncing request. Current synced period {}, peer synced "
              "period {}, peer light node history {}. Peer will be disconnected",
              peer->getId().abridged(), pbft_synced_period, packet.peer_pbft_chain_size,
              packet.initial_data->node_history);
        } else {
          logger_->error(
              "Light node {} would not be able to serve our syncing request. Current synced period {}, peer synced "
              "period {}, peer light node history {}. Peer will be disconnected",
              peer->getId().abridged(), pbft_synced_period, packet.peer_pbft_chain_size,
              packet.initial_data->node_history);
        }
        disconnect(peer->getId(), dev::p2p::UserReason);
        return;
      }
    }

    selected_peer->dag_level_ = packet.peer_dag_level;
    selected_peer->pbft_chain_size_ = packet.peer_pbft_chain_size;
    selected_peer->syncing_ = packet.peer_syncing;
    selected_peer->pbft_period_ = packet.peer_pbft_chain_size + 1;
    selected_peer->pbft_round_ = packet.peer_pbft_round;

    peers_state_->setPeerAsReadyToSendMessages(peer->getId(), selected_peer);

    logger_->debug(
        "Received initial status message from {}, network id {}, peer DAG max level {}, genesis {}, peer pbft chain "
        "size {}, peer syncing {}, peer pbft period {}, peer pbft round {}, node major version {}, node minor version "
        "{}, node patch version {}",
        peer->getId(), packet.initial_data->peer_chain_id, selected_peer->dag_level_, packet.initial_data->genesis_hash,
        selected_peer->pbft_chain_size_, selected_peer->syncing_, selected_peer->pbft_period_,
        selected_peer->pbft_round_, packet.initial_data->node_major_version, packet.initial_data->node_minor_version,
        packet.initial_data->node_patch_version);

  } else {  // Standard status packet
    if (!selected_peer) {
      logger_->error(
          "Received standard status packet from {}, without previously received initial status packet. Will be "
          "disconnected",
          peer->getId().abridged());
      disconnect(peer->getId(), dev::p2p::UserReason);
      return;
    }

    selected_peer->dag_level_ = packet.peer_dag_level;
    selected_peer->pbft_chain_size_ = packet.peer_pbft_chain_size;
    selected_peer->pbft_period_ = selected_peer->pbft_chain_size_ + 1;
    selected_peer->syncing_ = packet.peer_syncing;
    selected_peer->pbft_round_ = packet.peer_pbft_round;

    // TODO: Address malicious status
    if (!pbft_syncing_state_->isPbftSyncing()) {
      if (pbft_synced_period < selected_peer->pbft_chain_size_) {
        logger_->info("Restart PBFT chain syncing. Own synced PBFT at period {}, peer PBFT chain size {}",
                      pbft_synced_period, selected_peer->pbft_chain_size_);
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
        // TODO: this functionality is implemented in requestPbftNextVotesAtPeriodRound in ExtVotesPacketHandler
        const auto get_next_votes_packet =
            GetNextVotesBundlePacket{.peer_pbft_period = pbft_current_period, .peer_pbft_round = pbft_current_round};
        sealAndSend(selected_peer->getId(), SubprotocolPacketType::kGetNextVotesSyncPacket,
                    encodePacketRlp(get_next_votes_packet));
      }
    }
    selected_peer->last_status_pbft_chain_size_ = selected_peer->pbft_chain_size_.load();

    logger_->debug(
        "Received status message from {}, peer DAG max level {}, peer pbft chain size {}, peer syncing {}, peer pbft "
        "round {}",
        peer->getId(), selected_peer->dag_level_, selected_peer->pbft_chain_size_, selected_peer->syncing_,
        selected_peer->pbft_round_);
  }
}

}  // namespace taraxa::network::tarcap
