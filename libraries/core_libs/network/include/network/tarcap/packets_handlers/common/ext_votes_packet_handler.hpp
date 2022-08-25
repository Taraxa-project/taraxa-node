#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class Vote;
class PbftManager;
class PbftChain;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

/**
 * @brief ExtVotesPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that process pbft votes
 */
class ExtVotesPacketHandler : public PacketHandler {
 public:
  ExtVotesPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<VoteManager> vote_mgr, uint32_t vote_accepting_periods, const addr_t& node_addr,
                        const std::string& log_channel_name);

  virtual ~ExtVotesPacketHandler() = default;
  ExtVotesPacketHandler(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler(ExtVotesPacketHandler&&) = delete;
  ExtVotesPacketHandler& operator=(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler& operator=(ExtVotesPacketHandler&&) = delete;

  /**
   * @brief Validates standard vote
   *
   * @param vote to be validated
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateStandardVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Validates reward vote
   *
   * @param vote to be validated
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateRewardVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Validates next vote
   *
   * @param vote to be validated
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateNextSyncVote(const std::shared_ptr<Vote>& vote) const;

  void onNewPbftVotes(std::vector<std::shared_ptr<Vote>>&& votes);
  void sendPbftVotes(const dev::p2p::NodeID& peer_id, std::vector<std::shared_ptr<Vote>>&& votes,
                     bool is_next_votes = false);

 protected:
  /**
   * @brief Common validation for all types of votes
   *
   * @param vote to be validated
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateVote(const std::shared_ptr<Vote>& vote) const;

 protected:
  // Dpos contract delay - it is used to validate pbft period in votes -> does not make sense to accept vote
  // with vote period > current pbft period + kDposDelay as the valiation will fail
  const uint32_t kVoteAcceptingPeriods;

  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
};

}  // namespace taraxa::network::tarcap
