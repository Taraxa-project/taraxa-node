#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class Vote;
}  // namespace taraxa

namespace taraxa::network::tarcap {

/**
 * @brief VotesHandler is not a classic handler that would processing any incoming packets. It is a set of common
 *        functions that are used in packet handlers that somehow process pbft votes
 */
class VotesHandler : public PacketHandler {
 public:
  VotesHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
               const addr_t& node_addr, const std::string& log_channel_name);

  void onNewPbftVote(std::shared_ptr<Vote> const& vote);
  void sendPbftVote(dev::p2p::NodeID const& peer_id, std::shared_ptr<Vote> const& vote);

 protected:
  void sendPbftNextVotes(dev::p2p::NodeID const& peer_id,
                         std::vector<std::shared_ptr<Vote>> const& send_next_votes_bundle);
};

}  // namespace taraxa::network::tarcap
