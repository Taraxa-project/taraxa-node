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

void GetPbftSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) const {
  if (constexpr size_t required_size = 1; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void GetPbftSyncPacketHandler::process(const PacketData &packet_data,
                                       [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_tr_) << "Received GetPbftSyncPacket Block";

  const size_t height_to_sync = packet_data.rlp_[0].toInt();
  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  if (height_to_sync > my_chain_size) {
    // Node update peers PBFT chain size in status packet. Should not request syncing period bigger than pbft chain size
    std::ostringstream err_msg;
    err_msg << "Peer " << packet_data.from_node_id_ << " request syncing period start at " << height_to_sync
            << ". That's bigger than own PBFT chain size " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  size_t blocks_to_transfer = 0;
  auto will_be_synced = false;
  const auto total_sync_blocks_size = my_chain_size - height_to_sync + 1;
  if (total_sync_blocks_size <= network_sync_level_size_) {
    blocks_to_transfer = total_sync_blocks_size;
    will_be_synced = true;
  } else {
    blocks_to_transfer = network_sync_level_size_;
  }
  LOG(log_tr_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << packet_data.from_node_id_;

  sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer, will_be_synced);
}

// api for pbft syncing
void GetPbftSyncPacketHandler::sendPbftBlocks(dev::p2p::NodeID const &peer_id, size_t height_to_sync,
                                              size_t blocks_to_transfer, bool will_be_synced) {
  LOG(log_tr_) << "sendPbftBlocks: peer want to sync from pbft chain height " << height_to_sync
               << ", will send at most " << blocks_to_transfer << " pbft blocks to " << peer_id;

  uint64_t current_period = height_to_sync;
  while (current_period < height_to_sync + blocks_to_transfer) {
    bool last_block = (current_period == height_to_sync + blocks_to_transfer - 1);
    auto data = db_->getPeriodDataRaw(current_period);

    if (data.size() == 0) {
      LOG(log_er_) << "DB corrupted. Cannot find period " << current_period << " PBFT block in db";
      assert(false);
    }

    dev::RLPStream s;
    s.appendList(3);
    s << (will_be_synced && last_block);
    s << last_block;
    s.appendRaw(data);
    LOG(log_dg_) << "Sending PbftSyncPacket period " << current_period << " to " << peer_id;
    sealAndSend(peer_id, SubprotocolPacketType::PbftSyncPacket, std::move(s));
    current_period++;
  }
}

}  // namespace taraxa::network::tarcap
