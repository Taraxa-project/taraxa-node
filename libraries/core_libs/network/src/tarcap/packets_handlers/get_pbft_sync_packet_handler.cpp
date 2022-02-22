#include "network/tarcap/packets_handlers/get_pbft_sync_packet_handler.hpp"

#include "network/tarcap/shared_states/syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "storage/storage.hpp"
#include "vote/vote.hpp"

namespace taraxa::network::tarcap {

GetPbftSyncPacketHandler::GetPbftSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<PacketsStats> packets_stats,
                                                   std::shared_ptr<SyncingState> syncing_state,
                                                   std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DbStorage> db,
                                                   size_t network_sync_level_size, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_PBFT_SYNC_PH"),
      syncing_state_(std::move(syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      db_(std::move(db)),
      network_sync_level_size_(network_sync_level_size) {}

void GetPbftSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) {
  checkPacketRlpList(packet_data);

  if (size_t required_size = 1; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }

  // In case there is a type mismatch, one of the dev::RLPException's is thrown during further parsing
}

void GetPbftSyncPacketHandler::process(const PacketData &packet_data,
                                       [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_tr_) << "Received GetPbftSyncPacket Block";

  const size_t height_to_sync = packet_data.rlp_[0].toInt();
  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  size_t blocks_to_transfer = 0;
  if (my_chain_size >= height_to_sync) {
    blocks_to_transfer = std::min(network_sync_level_size_, (my_chain_size - (height_to_sync - 1)));
  }
  LOG(log_tr_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << packet_data.from_node_id_;

  sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer);
}

// api for pbft syncing
void GetPbftSyncPacketHandler::sendPbftBlocks(dev::p2p::NodeID const &peer_id, size_t height_to_sync,
                                              size_t blocks_to_transfer) {
  LOG(log_tr_) << "sendPbftBlocks: peer want to sync from pbft chain height " << height_to_sync
               << ", will send at most " << blocks_to_transfer << " pbft blocks to " << peer_id;
  uint64_t current_period = height_to_sync;
  if (blocks_to_transfer == 0) {
    LOG(log_dg_) << "Sending empty PbftSyncPacket to " << peer_id;
    sealAndSend(peer_id, SubprotocolPacketType::PbftSyncPacket, dev::RLPStream(0));
    return;
  }

  while (current_period < height_to_sync + blocks_to_transfer) {
    bool last_block = (current_period == height_to_sync + blocks_to_transfer - 1);
    auto data = db_->getPeriodDataRaw(current_period);

    if (data.size() == 0) {
      sealAndSend(peer_id, SubprotocolPacketType::PbftSyncPacket, dev::RLPStream(0));
      LOG(log_er_) << "Missing pbft block in db, send no pbft blocks to " << peer_id;
      return;
    }

    dev::RLPStream s;
    s.appendList(2);
    s << last_block;
    s.appendRaw(data);
    LOG(log_dg_) << "Sending PbftSyncPacket period " << current_period << " to " << peer_id;
    sealAndSend(peer_id, SubprotocolPacketType::PbftSyncPacket, std::move(s));
    current_period++;
  }
}

}  // namespace taraxa::network::tarcap
