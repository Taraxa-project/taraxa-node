#include "network/tarcap/packets_handlers/get_pbft_sync_packet_handler.hpp"

#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "storage/storage.hpp"
#include "vote/vote.hpp"

namespace taraxa::network::tarcap {

GetPbftSyncPacketHandler::GetPbftSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                   std::shared_ptr<PacketsStats> packets_stats,
                                                   std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                                   std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DbStorage> db,
                                                   size_t network_sync_level_size, const addr_t &node_addr,
                                                   bool is_light_node, uint64_t light_node_history)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_PBFT_SYNC_PH"),
      pbft_syncing_state_(std::move(pbft_syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      db_(std::move(db)),
      network_sync_level_size_(network_sync_level_size),
      is_light_node_(is_light_node),
      light_node_history_(light_node_history) {}

void GetPbftSyncPacketHandler::validatePacketRlpFormat(const PacketData &packet_data) const {
  if (constexpr size_t required_size = 1; packet_data.rlp_.itemCount() != required_size) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, packet_data.rlp_.itemCount(), required_size);
  }
}

void GetPbftSyncPacketHandler::process(const PacketData &packet_data,
                                       [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_tr_) << "Received GetPbftSyncPacket Block";

  const size_t height_to_sync = packet_data.rlp_[0].toInt();
  if (!pbft_syncing_state_->updatePeerAskingPeriod(packet_data.from_node_id_, height_to_sync)) {
    std::ostringstream err_msg;
    err_msg << "Peer " << packet_data.from_node_id_ << " request syncing asked previoud period " << height_to_sync;
    throw MaliciousPeerException(err_msg.str());
  }

  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  if (height_to_sync > my_chain_size) {
    // Node update peers PBFT chain size in status packet. Should not request syncing period bigger than pbft chain size
    std::ostringstream err_msg;
    err_msg << "Peer " << packet_data.from_node_id_ << " request syncing period start at " << height_to_sync
            << ". That's bigger than own PBFT chain size " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  if (is_light_node_ && height_to_sync + light_node_history_ <= my_chain_size) {
    std::ostringstream err_msg;
    err_msg << "Peer " << packet_data.from_node_id_ << " request syncing period start at " << height_to_sync
            << ". Light node does not have the data " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  size_t blocks_to_transfer = 0;
  auto pbft_chain_synced = false;
  const auto total_sync_blocks_size = my_chain_size - height_to_sync + 1;
  if (total_sync_blocks_size <= network_sync_level_size_) {
    blocks_to_transfer = total_sync_blocks_size;
    pbft_chain_synced = true;
  } else {
    blocks_to_transfer = network_sync_level_size_;
  }
  LOG(log_tr_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << packet_data.from_node_id_;

  sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer, pbft_chain_synced);
}

// api for pbft syncing
void GetPbftSyncPacketHandler::sendPbftBlocks(dev::p2p::NodeID const &peer_id, size_t height_to_sync,
                                              size_t blocks_to_transfer, bool pbft_chain_synced) {
  LOG(log_tr_) << "sendPbftBlocks: peer want to sync from pbft chain height " << height_to_sync
               << ", will send at most " << blocks_to_transfer << " pbft blocks to " << peer_id;

  for (auto block_period = height_to_sync; block_period < height_to_sync + blocks_to_transfer; block_period++) {
    bool last_block = (block_period == height_to_sync + blocks_to_transfer - 1);
    auto data = db_->getPeriodDataRaw(block_period);

    if (data.size() == 0) {
      LOG(log_er_) << "DB corrupted. Cannot find period " << block_period << " PBFT block in db";
      assert(false);
    }

    dev::RLPStream s;
    s.appendList(3);
    s << (pbft_chain_synced && last_block);
    s << last_block;
    s.appendRaw(data);
    LOG(log_dg_) << "Sending PbftSyncPacket period " << block_period << " to " << peer_id;
    sealAndSend(peer_id, SubprotocolPacketType::PbftSyncPacket, std::move(s));
  }
}

}  // namespace taraxa::network::tarcap
