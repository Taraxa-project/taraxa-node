#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftBlock;
class PbftChain;
class PbftManager;
class DagBlockManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PbftBlockPacketHandler final : public PacketHandler {
 public:
  PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                         const addr_t& node_addr);

  void onNewPbftBlock(const std::shared_ptr<PbftBlock>& pbft_block);
  void sendPbftBlock(const dev::p2p::NodeID& peer_id, const std::shared_ptr<PbftBlock>& pbft_block,
                     uint64_t pbft_chain_size);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PbftBlockPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;
};

}  // namespace taraxa::network::tarcap
