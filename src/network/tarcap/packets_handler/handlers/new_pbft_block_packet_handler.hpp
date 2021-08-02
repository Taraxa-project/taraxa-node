#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class PbftBlock;
class PbftChain;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class NewPbftBlockPacketHandler : public PacketHandler {
 public:
  NewPbftBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PbftChain> pbft_chain,
                            const addr_t &node_addr = {});

  void onNewPbftBlock(PbftBlock const &pbft_block);
  void sendPbftBlock(dev::p2p::NodeID const &peer_id, PbftBlock const &pbft_block, uint64_t pbft_chain_size);

 private:
  void process(const PacketData &packet_data, const dev::RLP &packet_rlp) override;

  std::shared_ptr<PbftChain> pbft_chain_;
};

}  // namespace taraxa::network::tarcap