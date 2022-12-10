#pragma once

#include "packet_handler.hpp"

namespace taraxa {
class Vote;
class PbftManager;
class PbftChain;
class PbftBlock;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

/**
 * @brief ExtVotesPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that process pbft votes
 */
class ExtVotesPacketHandler : public PacketHandler {
 public:
  ExtVotesPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<AllPacketsStats> packets_stats,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<VoteManager> vote_mgr, const NetworkConfig& net_config, const addr_t& node_addr,
                        const std::string& log_channel_name);

  virtual ~ExtVotesPacketHandler() = default;
  ExtVotesPacketHandler(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler(ExtVotesPacketHandler&&) = delete;
  ExtVotesPacketHandler& operator=(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler& operator=(ExtVotesPacketHandler&&) = delete;

  /**
   * @brief Process standard vote
   *
   * @param vote
   * @param pbft_block
   * @param peer
   * @param validate_max_round_step
   * @return if vote was successfully processed, otherwise false
   */
  bool processStandardVote(const std::shared_ptr<Vote>& vote, const std::shared_ptr<PbftBlock>& pbft_block,
                           const std::shared_ptr<TaraxaPeer>& peer, bool validate_max_round_step);

  /**
   * @brief Process reward vote
   *
   * @param vote
   * @return if vote was successfully processed, otherwise false
   */
  bool processRewardVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Process next sync vote
   *
   * @param vote
   * @param pbft_block
   * @return if vote was successfully processed, otherwise false
   */
  bool processNextSyncVote(const std::shared_ptr<Vote>& vote, const std::shared_ptr<PbftBlock>& pbft_block) const;

  /**
   * @brief Sends pbft votes to connected peers
   *
   * @param votes Votes to send
   * @param rebroadcast if rebroadcast is true, all votes are resent to all peers
   */
  void onNewPbftVotes(std::vector<std::shared_ptr<Vote>>&& votes, bool rebroadcast = false);
  void sendPbftVotes(const std::shared_ptr<TaraxaPeer>& peer, std::vector<std::shared_ptr<Vote>>&& votes,
                     bool is_next_votes = false);

  void onNewPbftVote(const std::shared_ptr<Vote>& vote, const std::shared_ptr<PbftBlock>& block);
  void sendPbftVote(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<Vote>& vote,
                    const std::shared_ptr<PbftBlock>& block);

 protected:
  /**
   * @brief Validates standard vote
   *
   * @param vote to be validated
   * @param peer
   * @param validate_max_round_step validate also max round and step
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateStandardVote(const std::shared_ptr<Vote>& vote,
                                                    const std::shared_ptr<TaraxaPeer>& peer,
                                                    bool validate_max_round_step);

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

  /**
   * @brief Common validation for all types of votes
   *
   * @param vote to be validated
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateVote(const std::shared_ptr<Vote>& vote) const;

  /**
   * @brief Validates provided vote if voted value == provided block
   *
   * @param vote
   * @param pbft_block
   * @return true if validation successful, otherwise false
   */
  bool validateVoteAndBlock(const std::shared_ptr<Vote>& vote, const std::shared_ptr<PbftBlock>& pbft_block) const;

 protected:
  const size_t kMaxVotesInPacket{1000};
  const size_t kVotePacketWithBlockSize{3};

  // Dpos contract delay - it is used to validate pbft period in votes -> does not make sense to accept vote
  // with vote period > current pbft period + kDposDelay as the valiation will fail
  const PbftPeriod kVoteAcceptingPeriods;
  const PbftRound kVoteAcceptingRounds;
  const PbftStep kVoteAcceptingSteps;
  constexpr static std::chrono::seconds kSyncRequestInterval = std::chrono::seconds(10);

  mutable std::chrono::system_clock::time_point last_votes_sync_request_time_;
  mutable std::chrono::system_clock::time_point last_pbft_block_sync_request_time_;

  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
};

}  // namespace taraxa::network::tarcap
