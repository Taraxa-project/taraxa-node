#include "new_pbft_block_packet_handler.hpp"

#include <libp2p/Common.h>

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"

namespace taraxa::network::tarcap {

NewPbftBlockPacketHandler::NewPbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                     std::shared_ptr<PacketsStats> packets_stats,
                                                     std::shared_ptr<PbftChain> pbft_chain,
                                                     std::shared_ptr<PbftManager> pbft_mgr, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "NEW_PBFT_BLOCK_PH"),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)) {}

void NewPbftBlockPacketHandler::process(const dev::RLP &packet_rlp, [[maybe_unused]] const PacketData &packet_data,

                                        const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "In NewPbftBlockPacket";

  auto pbft_block = std::make_shared<PbftBlock>(packet_rlp[0]);
  const uint64_t peer_pbft_chain_size = packet_rlp[1].toInt();
  LOG(log_dg_) << "Receive proposed PBFT Block " << pbft_block->getBlockHash().abridged()
               << ", Peer PBFT Chain size: " << peer_pbft_chain_size;

  peer->markPbftBlockAsKnown(pbft_block->getBlockHash());
  if (peer_pbft_chain_size > peer->pbft_chain_size_) {
    peer->pbft_chain_size_ = peer_pbft_chain_size;
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

void NewPbftBlockPacketHandler::onNewPbftBlock(PbftBlock const &pbft_block) {
  std::vector<dev::p2p::NodeID> peers_to_send;
  const auto my_chain_size = pbft_chain_->getPbftChainSize();

  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isPbftBlockKnown(pbft_block.getBlockHash()) && !peer.second->syncing_) {
      if (!peer.second->isBlockKnown(pbft_block.getPivotDagBlockHash())) {
        LOG(log_wr_) << "sending PbftBlock " << pbft_block.getBlockHash() << " with missing dag anchor"
                     << pbft_block.getPivotDagBlockHash() << " to " << peer.first;
      }
      peers_to_send.push_back(peer.first);
    }
  }

  for (auto const &peer : peers_to_send) {
    sendPbftBlock(peer, pbft_block, my_chain_size);
  }
}

void NewPbftBlockPacketHandler::sendPbftBlock(dev::p2p::NodeID const &peer_id, PbftBlock const &pbft_block,
                                              uint64_t pbft_chain_size) {
  LOG(log_dg_) << "sendPbftBlock " << pbft_block.getBlockHash() << " to " << peer_id;
  dev::RLPStream s(2);
  pbft_block.streamRLP(s, true);
  s << pbft_chain_size;
  sealAndSend(peer_id, NewPbftBlockPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap