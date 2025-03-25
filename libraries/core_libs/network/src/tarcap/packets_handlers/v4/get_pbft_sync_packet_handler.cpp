#include "network/tarcap/packets_handlers/v4/get_pbft_sync_packet_handler.hpp"

#include "network/tarcap/packets/latest/pbft_sync_packet.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"
#include "storage/storage.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap::v4 {

void GetPbftSyncPacketHandler::process(const threadpool::PacketData& packet_data,
                                       const std::shared_ptr<TaraxaPeer>& peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<GetPbftSyncPacket>(packet_data.rlp_);

  LOG(log_tr_) << "Received GetPbftSyncPacket Block";

  // Here need PBFT chain size, not synced period since synced blocks has not verified yet.
  const size_t my_chain_size = pbft_chain_->getPbftChainSize();
  if (packet.height_to_sync > my_chain_size) {
    // Node update peers PBFT chain size in status packet. Should not request syncing period bigger than pbft chain size
    std::ostringstream err_msg;
    err_msg << "Peer " << peer->getId() << " request syncing period start at " << packet.height_to_sync
            << ". That's bigger than own PBFT chain size " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  if (kConf.is_light_node && packet.height_to_sync + kConf.light_node_history <= my_chain_size) {
    std::ostringstream err_msg;
    err_msg << "Peer " << peer->getId() << " request syncing period start at " << packet.height_to_sync
            << ". Light node does not have the data " << my_chain_size;
    throw MaliciousPeerException(err_msg.str());
  }

  size_t blocks_to_transfer = 0;
  auto pbft_chain_synced = false;
  const auto total_period_data_size = my_chain_size - packet.height_to_sync + 1;
  if (total_period_data_size <= kConf.network.sync_level_size) {
    blocks_to_transfer = total_period_data_size;
    pbft_chain_synced = true;
  } else {
    blocks_to_transfer = kConf.network.sync_level_size;
  }
  LOG(log_tr_) << "Will send " << blocks_to_transfer << " PBFT blocks to " << peer->getId();

  sendPbftBlocks(peer, packet.height_to_sync, blocks_to_transfer, pbft_chain_synced);
}

}  // namespace taraxa::network::tarcap::v4
