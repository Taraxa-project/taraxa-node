#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"

#include <libp2p/Common.h>

#include "network/tarcap/shared_states/syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::tarcap {

PbftBlockPacketHandler::PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<PacketsStats> packets_stats,
                                               std::shared_ptr<SyncingState> syncing_state,
                                               std::shared_ptr<PbftChain> pbft_chain,
                                               std::shared_ptr<PbftManager> pbft_mgr,
                                               std::shared_ptr<DagBlockManager> dag_blk_mgr, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "PBFT_BLOCK_PH"),
      syncing_state_(std::move(syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)) {}

void PbftBlockPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "In PbftBlockPacket";

  auto pbft_block = std::make_shared<PbftBlock>(packet_data.rlp_[0]);
  const uint64_t peer_pbft_chain_size = packet_data.rlp_[1].toInt();
  LOG(log_dg_) << "Receive proposed PBFT Block " << pbft_block->getBlockHash().abridged()
               << ", Peer PBFT Chain size: " << peer_pbft_chain_size;

  peer->markPbftBlockAsKnown(pbft_block->getBlockHash());
  if (peer_pbft_chain_size > peer->pbft_chain_size_) {
    peer->pbft_chain_size_ = peer_pbft_chain_size;
  }

  // Ignore any pbft block that is missing dag anchor
  if (!dag_blk_mgr_->isDagBlockKnown(pbft_block->getPivotDagBlockHash())) {
    // If we are not syncing and missing anchor block, disconnect the node
    if (!syncing_state_->is_pbft_syncing()) {
      LOG(log_wr_) << "Pbft block" << pbft_block->getBlockHash() << " missing dag anchor "
                   << pbft_block->getPivotDagBlockHash() << " . Peer " << packet_data.from_node_id_.abridged()
                   << " will be disconnected.";
      disconnect(packet_data.from_node_id_, dev::p2p::UserReason);
    }
    return;
    // TODO: malicious peer handling
  }

  const auto pbft_synced_period = pbft_mgr_->pbftSyncingPeriod();
  if (pbft_synced_period >= pbft_block->getPeriod()) {
    LOG(log_dg_) << "Drop new PBFT block " << pbft_block->getBlockHash().abridged() << " at period "
                 << pbft_block->getPeriod() << ", own PBFT chain has synced at period " << pbft_synced_period;
    return;
  }

  // Synchronization point in case multiple threads are processing the same block at the same time
  if (!pbft_chain_->pushUnverifiedPbftBlock(pbft_block)) {
    LOG(log_dg_) << "Drop new PBFT block " << pbft_block->getBlockHash().abridged() << " at period "
                 << pbft_block->getPeriod() << " -> already inserted in unverified queue ";
    return;
  }

  onNewPbftBlock(*pbft_block);
}

void PbftBlockPacketHandler::onNewPbftBlock(PbftBlock const &pbft_block) {
  std::vector<std::shared_ptr<TaraxaPeer>> peers_to_send;
  std::vector<std::shared_ptr<TaraxaPeer>> peers_missing_anchor;
  const auto my_chain_size = pbft_chain_->getPbftChainSize();

  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isPbftBlockKnown(pbft_block.getBlockHash()) && !peer.second->syncing_) {
      if (peer.second->isDagBlockKnown(pbft_block.getPivotDagBlockHash())) {
        peers_to_send.emplace_back(peer.second);
      } else {
        peers_missing_anchor.emplace_back(peer.second);
      }
    }
  }

  for (auto const &peer : peers_to_send) {
    sendPbftBlock(peer->getId(), pbft_block, my_chain_size);
    peer->markPbftBlockAsKnown(pbft_block.getBlockHash());
  }

  peers_to_send.clear();

  // Check again for peers missing anchor
  for (auto const &peer : peers_missing_anchor) {
    if (peer->isDagBlockKnown(pbft_block.getPivotDagBlockHash())) {
      peers_to_send.emplace_back(peer);
    } else {
      LOG(log_wr_) << "PbftBlock " << pbft_block.getBlockHash() << " dag anchor is not broadcast to peer yet "
                   << pbft_block.getPivotDagBlockHash() << " to " << peer->getId();
    }
  }

  for (auto const &peer : peers_to_send) {
    sendPbftBlock(peer->getId(), pbft_block, my_chain_size);
    peer->markPbftBlockAsKnown(pbft_block.getBlockHash());
  }
}

void PbftBlockPacketHandler::sendPbftBlock(dev::p2p::NodeID const &peer_id, PbftBlock const &pbft_block,
                                           uint64_t pbft_chain_size) {
  LOG(log_dg_) << "sendPbftBlock " << pbft_block.getBlockHash() << " to " << peer_id;
  dev::RLPStream s(2);
  pbft_block.streamRLP(s, true);
  s << pbft_chain_size;
  sealAndSend(peer_id, PbftBlockPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
