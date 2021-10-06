#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class Vote;
}  // namespace taraxa

namespace taraxa::network::tarcap {

/**
 * @brief ExtVotesPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that process pbft votes
 */
class ExtVotesPacketHandler : public PacketHandler {
 public:
  ExtVotesPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                        const addr_t& node_addr, const std::string& log_channel_name);

  virtual ~ExtVotesPacketHandler() = default;

  void onNewPbftVote(std::shared_ptr<Vote> const& vote);
  void sendPbftVote(dev::p2p::NodeID const& peer_id, std::shared_ptr<Vote> const& vote);

 protected:
  void sendPbftNextVotes(dev::p2p::NodeID const& peer_id,
                         std::vector<std::shared_ptr<Vote>> const& send_next_votes_bundle);
};

}  // namespace taraxa::network::tarcap
