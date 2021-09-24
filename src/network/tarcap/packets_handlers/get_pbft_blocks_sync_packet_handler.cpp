#include "get_pbft_blocks_sync_packet_handler.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/vote.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "storage/db_storage.hpp"

namespace taraxa::network::tarcap {

GetPbftBlocksSyncPacketHandler::GetPbftBlocksSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                               std::shared_ptr<PacketsStats> packets_stats,
                                                               std::shared_ptr<SyncingState> syncing_state,
                                                               std::shared_ptr<PbftChain> pbft_chain,
                                                               std::shared_ptr<DbStorage> db,
                                                               size_t network_sync_level_size, const addr_t &node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "GET_PBFT_BLOCK_PH"),
      syncing_state_(std::move(syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      db_(std::move(db)),
      network_sync_level_size_(network_sync_level_size) {}

void GetPbftBlocksSyncPacketHandler::process(const PacketData &packet_data,
                                             [[maybe_unused]] const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "Received GetPbftBlocksSyncPacket Block";

  const size_t height_to_sync = packet_data.rlp_[0].toInt();
  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  size_t blocks_to_transfer = 0;
  if (my_chain_size >= height_to_sync) {
    blocks_to_transfer = std::min(network_sync_level_size_, (my_chain_size - (height_to_sync - 1)));
  }
  LOG(log_dg_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << packet_data.from_node_id_;

  // TODO: this logic could go tarcap::interpretCapabilityPacket packet, but it might not be needed anymore !!!

  //   Stop syncing if too many sync requests or if the rest of the node is busy
  //   if (blocks_to_transfer > 0) {
  // if (syncing_tp_.num_pending_tasks() >= MAX_SYNCING_NODES) {
  //   LOG(log_wr_) << "Node is already serving max syncing nodes, host " << packet_data.from_node_id_.abridged()
  //                << " will be disconnected";
  //   host->disconnect(packet_data.from_node_id_, p2p::UserReason);
  //   break;
  // }
  //   }
  // If blocks_to_transfer is 0, send peer empty PBFT blocks for talking to peer syncing has completed
  //   syncing_tp_.post([host, packet_data.from_node_id_, height_to_sync, blocks_to_transfer, this] {
  //     bool send_blocks = true;
  //     if (blocks_to_transfer > 0) {
  //       uint32_t total_wait_time = 0;
  //       while (tp_.num_pending_tasks() > MAX_NETWORK_QUEUE_TO_DROP_SYNCING) {
  //         uint32_t delay_time = MAX_TIME_TO_WAIT_FOR_QUEUE_TO_CLEAR_MS / 10;
  //         thisThreadSleepForMilliSeconds(delay_time);
  //         total_wait_time += delay_time;
  //         if (total_wait_time >= MAX_TIME_TO_WAIT_FOR_QUEUE_TO_CLEAR_MS) {
  //           LOG(log_er_) << "Node is busy with " << tp_.num_pending_tasks() << " network packets. Host "
  //                        << packet_data.from_node_id_.abridged() << " will be disconnected";
  //           host->disconnect(packet_data.from_node_id_, p2p::UserReason);
  //           send_blocks = false;
  //           break;
  //         }
  //       }
  //     }
  //     if (send_blocks) sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer);
  //   });
  sendPbftBlocks(packet_data.from_node_id_, height_to_sync, blocks_to_transfer);
}

// api for pbft syncing
void GetPbftBlocksSyncPacketHandler::sendPbftBlocks(dev::p2p::NodeID const &peer_id, size_t height_to_sync,
                                                    size_t blocks_to_transfer) {
  LOG(log_dg_) << "sendPbftBlocks: peer want to sync from pbft chain height " << height_to_sync
               << ", will send at most " << blocks_to_transfer << " pbft blocks to " << peer_id;
  uint64_t current_period = height_to_sync;
  if (blocks_to_transfer == 0) {
    sealAndSend(peer_id, SubprotocolPacketType::PbftBlocksSyncPacket, RLPStream(0));
    return;
  }

  while (current_period < height_to_sync + blocks_to_transfer) {
    bool last_block = (current_period == height_to_sync + blocks_to_transfer - 1);
    auto data = db_->getPeriodDataRaw(current_period);

    if (data.size() == 0) {
      sealAndSend(peer_id, SubprotocolPacketType::PbftBlocksSyncPacket, RLPStream(0));
      LOG(log_er_) << "Missing pbft block in db, send no pbft blocks to " << peer_id;
      return;
    }

    RLPStream s;
    s.appendList(2);
    s << last_block;
    s.appendRaw(data);
    LOG(log_dg_) << "Sending PbftBlocksSyncPacket period " << current_period << " to " << peer_id;
    sealAndSend(peer_id, SubprotocolPacketType::PbftBlocksSyncPacket, std::move(s));
    current_period++;
  }
}

}  // namespace taraxa::network::tarcap
