#include "network/tarcap/packets_handlers/latest/common/ext_syncing_packet_handler.hpp"

#include "network/tarcap/packets/latest/get_dag_sync_packet.hpp"
#include "network/tarcap/packets/latest/get_pbft_sync_packet.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::tarcap {

ExtSyncingPacketHandler::ExtSyncingPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                 std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                                 std::shared_ptr<PbftChain> pbft_chain,
                                                 std::shared_ptr<PbftManager> pbft_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                                 const addr_t &node_addr, const std::string &log_channel_name)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
      pbft_syncing_state_(std::move(pbft_syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      db_(std::move(db)) {}

void ExtSyncingPacketHandler::requestPendingDagBlocks(std::shared_ptr<TaraxaPeer> peer) {
  if (!peer) {
    peer = peers_state_->getMaxChainPeer(pbft_mgr_, [](const std::shared_ptr<TaraxaPeer> &peer) {
      if (peer->peer_dag_synced_ || !peer->dagSyncingAllowed()) {
        return false;
      }
      return true;
    });
    if (!peer) {
      LOG(this->log_nf_) << "requestPendingDagBlocks not possible since no peers are matching conditions";
      return;
    }
  }

  if (!peer) {
    LOG(this->log_nf_) << "requestPendingDagBlocks not possible since no connected peers";
    return;
  }

  // This prevents ddos requesting dag blocks. We can only request this one time from one peer.
  if (peer->peer_dag_synced_) {
    LOG(this->log_nf_) << "requestPendingDagBlocks not possible since already requested for peer";
    return;
  }

  // Only request dag blocks if periods are matching
  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (pbft_sync_period == peer->pbft_chain_size_) {
    // This prevents parallel requests
    if (bool b = false; !peer->peer_dag_syncing_.compare_exchange_strong(b, !b)) {
      LOG(this->log_nf_) << "requestPendingDagBlocks not possible since already requesting for peer";
      return;
    }
    LOG(this->log_nf_) << "Request pending blocks from peer " << peer->getId();
    std::vector<blk_hash_t> known_non_finalized_blocks;
    auto [period, blocks] = dag_mgr_->getNonFinalizedBlocks();
    for (auto &level_blocks : blocks) {
      for (auto &block : level_blocks.second) {
        known_non_finalized_blocks.emplace_back(block);
      }
    }

    requestDagBlocks(peer->getId(), std::move(known_non_finalized_blocks), period);
  }
}

void ExtSyncingPacketHandler::requestDagBlocks(const dev::p2p::NodeID &_nodeID, std::vector<blk_hash_t> &&blocks,
                                               PbftPeriod period) {
  this->sealAndSend(_nodeID, SubprotocolPacketType::kGetDagSyncPacket,
                    encodePacketRlp(GetDagSyncPacket{period, std::move(blocks)}));
}

}  // namespace taraxa::network::tarcap
