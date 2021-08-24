#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftBlock;
class PbftChain;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class NewPbftBlockPacketHandler : public PacketHandler {
 public:
  NewPbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                            std::shared_ptr<PbftChain> pbft_chain, const addr_t& node_addr = {});

  virtual ~NewPbftBlockPacketHandler() = default;

  void onNewPbftBlock(PbftBlock const& pbft_block);
  void sendPbftBlock(dev::p2p::NodeID const& peer_id, PbftBlock const& pbft_block, uint64_t pbft_chain_size);

 private:
  void process(const dev::RLP& packet_rlp, const PacketData& packet_data, const std::shared_ptr<dev::p2p::Host>& host,
               const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftChain> pbft_chain_;
};

}  // namespace taraxa::network::tarcap