#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;

class GetPbftBlockPacketHandler : public PacketHandler {
 public:
  GetPbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                            std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                            std::shared_ptr<DbStorage> db, size_t network_sync_level_size,
                            const addr_t& node_addr = {});

  void sendPbftBlocks(dev::p2p::NodeID const& peer_id, size_t height_to_sync, size_t blocks_to_transfer);

 private:
  void process(const dev::RLP& packet_rlp, const PacketData& packet_data, const std::shared_ptr<dev::p2p::Host>& host,
               const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DbStorage> db_;

  size_t network_sync_level_size_;

  // When checking if size limit exceeds MAX_PACKET_SIZE there are few bytes or rlp structure that is added
  // for the pbft block and dag block. This should be just a couple of bytes but we enforece even stricter 128
  // limit
  static constexpr int RLP_OVERHEAD = 128;
};

}  // namespace taraxa::network::tarcap