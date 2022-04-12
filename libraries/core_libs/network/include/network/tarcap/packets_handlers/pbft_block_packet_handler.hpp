#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftBlock;
class PbftChain;
class PbftManager;
class VoteManager;
class DagBlockManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PbftBlockPacketHandler final : public PacketHandler {
 public:
  PbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                         std::shared_ptr<VoteManager> vote_mgr, const addr_t& node_addr);

  void onNewPbftBlock(PbftBlock const& pbft_block);
  void sendPbftBlock(dev::p2p::NodeID const& peer_id, PbftBlock const& pbft_block);

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<VoteManager> vote_mgr_;
};

}  // namespace taraxa::network::tarcap
