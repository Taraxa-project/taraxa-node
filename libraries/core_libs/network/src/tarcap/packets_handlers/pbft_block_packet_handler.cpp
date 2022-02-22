#include "network/tarcap/packets_handlers/pbft_block_packet_handler.hpp"

#include <libp2p/Common.h>

#include "network/tarcap/shared_states/syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::tarcap {

PbftBlockPacketHandler::PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<PacketsStats> packets_stats,
                                               std::shared_ptr<PbftChain> pbft_chain,
                                               std::shared_ptr<PbftManager> pbft_mgr, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "PBFT_BLOCK_PH"),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)) {}

void PbftBlockPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) {
  checkPacketRlpList(packet_data);

  if (size_t required_size = 2; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }

  // In case there is a type mismatch, one of the dev::RLPException's is thrown during further parsing
}

void PbftBlockPacketHandler::process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_tr_) << "In PbftBlockPacket";

  auto pbft_block = std::make_shared<PbftBlock>(packet_data.rlp_[0]);
  const uint64_t peer_pbft_chain_size = packet_data.rlp_[1].toInt();
  LOG(log_tr_) << "Receive proposed PBFT Block " << pbft_block->getBlockHash().abridged()
               << ", Peer PBFT Chain size: " << peer_pbft_chain_size << " from " << peer->getId();

  peer->markPbftBlockAsKnown(pbft_block->getBlockHash());
  if (peer_pbft_chain_size > peer->pbft_chain_size_) {
    peer->pbft_chain_size_ = peer_pbft_chain_size;
  }

  const auto pbft_synced_period = pbft_mgr_->pbftSyncingPeriod();
  if (pbft_synced_period >= pbft_block->getPeriod()) {
    LOG(log_tr_) << "Drop new PBFT block " << pbft_block->getBlockHash().abridged() << " at period "
                 << pbft_block->getPeriod() << ", own PBFT chain has synced at period " << pbft_synced_period;
    return;
  }

  // Synchronization point in case multiple threads are processing the same block at the same time
  if (!pbft_chain_->pushUnverifiedPbftBlock(pbft_block)) {
    LOG(log_tr_) << "Drop new PBFT block " << pbft_block->getBlockHash().abridged() << " at period "
                 << pbft_block->getPeriod() << " -> already inserted in unverified queue ";
    return;
  }

  LOG(log_dg_) << "Receive proposed PBFT Block " << pbft_block->getBlockHash().abridged()
               << ", Peer PBFT Chain size: " << peer_pbft_chain_size << " from " << peer->getId();

  onNewPbftBlock(*pbft_block);
}

void PbftBlockPacketHandler::onNewPbftBlock(PbftBlock const &pbft_block) {
  std::vector<std::shared_ptr<TaraxaPeer>> peers_to_send;
  const auto my_chain_size = pbft_chain_->getPbftChainSize();
  std::string peers_to_log;

  for (auto const &peer : peers_state_->getAllPeers()) {
    if (!peer.second->isPbftBlockKnown(pbft_block.getBlockHash()) && !peer.second->syncing_) {
      if (!peer.second->isDagBlockKnown(pbft_block.getPivotDagBlockHash())) {
        LOG(log_tr_) << "sending PbftBlock " << pbft_block.getBlockHash() << " with missing dag anchor"
                     << pbft_block.getPivotDagBlockHash() << " to " << peer.first;
      }
      peers_to_send.emplace_back(peer.second);
      peers_to_log += peer.second->getId().abridged();
    }
  }

  LOG(log_dg_) << "sendPbftBlock " << pbft_block.getBlockHash() << " to " << peers_to_log;
  for (auto const &peer : peers_to_send) {
    sendPbftBlock(peer->getId(), pbft_block, my_chain_size);
    peer->markPbftBlockAsKnown(pbft_block.getBlockHash());
  }
}

void PbftBlockPacketHandler::sendPbftBlock(dev::p2p::NodeID const &peer_id, PbftBlock const &pbft_block,
                                           uint64_t pbft_chain_size) {
  LOG(log_tr_) << "sendPbftBlock " << pbft_block.getBlockHash() << " to " << peer_id;
  dev::RLPStream s(2);
  pbft_block.streamRLP(s, true);
  s << pbft_chain_size;
  sealAndSend(peer_id, PbftBlockPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
