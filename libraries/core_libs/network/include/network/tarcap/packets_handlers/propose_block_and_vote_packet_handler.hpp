#pragma once

#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa {
class PbftBlock;
class PbftChain;
class PbftManager;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class ProposeBlockAndVotePacketHandler final : public ExtVotesPacketHandler {
 public:
  ProposeBlockAndVotePacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                         std::shared_ptr<VoteManager> vote_mgr, const NetworkConfig& net_config, const addr_t& node_addr);

  void onNewProposeBlockAndVote(const std::shared_ptr<PbftBlock> &proposed_block, const std::shared_ptr<Vote> &propose_vote);
  void sendProposeBlockAndVote(const dev::p2p::NodeID& peer_id, const std::shared_ptr<PbftBlock>& pbft_block,
                               const std::shared_ptr<Vote> &propose_vote, uint64_t pbft_chain_size);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::ProposeBlockAndVotePacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  bool validateProposeBlockAndVote(const std::shared_ptr<PbftBlock> &proposed_block, const std::shared_ptr<Vote> &propose_vote);

  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<VoteManager> vote_mgr_;
};

}  // namespace taraxa::network::tarcap
