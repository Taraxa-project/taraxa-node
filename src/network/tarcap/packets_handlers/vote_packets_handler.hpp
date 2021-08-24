#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftManager;
class VoteManager;
class NextVotesForPreviousRound;
class Vote;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class VotePacketsHandler : public PacketHandler {
 public:
  VotePacketsHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                     std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<VoteManager> vote_mgr,
                     std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr, std::shared_ptr<DbStorage> db,
                     const addr_t& node_addr = {});

  virtual ~VotePacketsHandler() = default;

  void sendPbftVote(dev::p2p::NodeID const& peer_id, Vote const& vote);
  void onNewPbftVote(Vote const& vote);
  void sendPbftNextVotes(dev::p2p::NodeID const& peer_id, std::vector<Vote> const& send_next_votes_bundle);
  void broadcastPreviousRoundNextVotesBundle();

 private:
  void process(const dev::RLP& packet_rlp, const PacketData& packet_data, const std::shared_ptr<dev::p2p::Host>& host,
               const std::shared_ptr<TaraxaPeer>& peer) override;

  inline void processPbftVotePacket(const dev::RLP& packet_rlp, const PacketData& packet_data,
                                    const std::shared_ptr<TaraxaPeer>& peer);
  inline void processGetPbftNextVotePacket(const dev::RLP& packet_rlp, const PacketData& packet_data,
                                           const std::shared_ptr<TaraxaPeer>& peer);
  inline void processPbftNextVotesPacket(const dev::RLP& packet_rlp, const PacketData& packet_data,
                                         const std::shared_ptr<dev::p2p::Host>& host,
                                         const std::shared_ptr<TaraxaPeer>& peer);

  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap