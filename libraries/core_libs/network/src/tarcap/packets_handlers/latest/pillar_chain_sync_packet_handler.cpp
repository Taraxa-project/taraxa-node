#include "network/tarcap/packets_handlers/latest/pillar_chain_sync_packet_handler.hpp"

#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::network::tarcap {

PillarChainSyncPacketHandler::PillarChainSyncPacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr,
                    logs_prefix + "PILLAR_CHAIN_SYNC_PH") {}

void PillarChainSyncPacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kPillarChainSyncPacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kPillarChainSyncPacketSize);
  }
}

void PillarChainSyncPacketHandler::process(const threadpool::PacketData &packet_data,
                                           const std::shared_ptr<TaraxaPeer> &peer) {
  // TODO: there could be the same protection as in pbft syncing that only requested bundle packet is accepted
  //  // Note: no need to consider possible race conditions due to concurrent processing as it is
  //  // disabled on priority_queue blocking dependencies level
  //  const auto syncing_peer = pbft_syncing_state_->syncingPeer();
  //  if (!syncing_peer) {
  //    LOG(log_wr_) << "PbftSyncPacket received from unexpected peer " << packet_data.from_node_id_.abridged()
  //                 << " but there is no current syncing peer set";
  //    return;
  //  }
  //
  //  if (syncing_peer->getId() != packet_data.from_node_id_) {
  //    LOG(log_wr_) << "PbftSyncPacket received from unexpected peer " << packet_data.from_node_id_.abridged()
  //                 << " current syncing peer " << syncing_peer->getId().abridged();
  //    return;
  //  }

  auto pillar_block_data = util::rlp_dec<pillar_chain::PillarBlockData>(packet_data.rlp_);

  if (!pillar_chain_manager_->isValidPillarBlock(pillar_block_data.block)) {
    LOG(log_er_) << "Invalid sync pillar block received from peer " << peer->getId();
    return;
  }

  // Validate signatures
  // TODO: this will not work for light nodes because pbft might already be too far ahead...
  //  for (const auto signature: pillar_block_data.signatures) {
  //
  //  }

  if (!pillar_chain_manager_->pushPillarBlock(pillar_block_data)) {
    LOG(log_er_) << "Unable to push sync pillar block received from peer " << peer->getId();
    return;
  }
}

}  // namespace taraxa::network::tarcap
