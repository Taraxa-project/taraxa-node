#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class PbftChain;
class PbftManager;
class VoteManager;
class Vote;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;

class PbftVotePacketHandler : public PacketHandler {
 public:
  PbftVotePacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<SyncingState> syncing_state,
                        std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                        std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                        const addr_t& node_addr = {});

  void sendPbftVote(dev::p2p::NodeID const& peerID, Vote const& vote);
  void onNewPbftVote(Vote const& vote);

 private:
  void process(const PacketData& packet_data, const dev::RLP& packet_rlp) override;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap