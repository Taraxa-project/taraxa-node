#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class Vote;
class PbftManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

/**
 * @brief ExtVotesPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that process pbft votes
 */
class ExtVotesPacketHandler : public PacketHandler {
 public:
  ExtVotesPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                        std::shared_ptr<PbftManager> pbft_mgr, const addr_t& node_addr,
                        const std::string& log_channel_name);

  virtual ~ExtVotesPacketHandler() = default;
  ExtVotesPacketHandler(const ExtVotesPacketHandler&) = default;
  ExtVotesPacketHandler(ExtVotesPacketHandler&&) = default;
  ExtVotesPacketHandler& operator=(const ExtVotesPacketHandler&) = default;
  ExtVotesPacketHandler& operator=(ExtVotesPacketHandler&&) = default;

  void onNewPbftVotes(std::vector<std::shared_ptr<Vote>>&& votes);
  void sendPbftVotes(const dev::p2p::NodeID& peer_id, std::vector<std::shared_ptr<Vote>>&& votes,
                     bool is_next_votes = false);

 private:
  std::shared_ptr<PbftManager> pbft_mgr_;
};

}  // namespace taraxa::network::tarcap
