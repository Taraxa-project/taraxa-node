#pragma once

#include "network/tarcap/packets/latest/get_pbft_sync_packet.hpp"
#include "network/tarcap/packets/latest/votes_bundle_packet.hpp"
#include "network/tarcap/packets_handlers/latest/common/exceptions.hpp"
#include "packet_handler.hpp"
#include "pbft/pbft_manager.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief ExtVotesPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that process pbft votes
 */
class ExtVotesPacketHandler : public PacketHandler {
 public:
  ExtVotesPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                        std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                        std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                        std::shared_ptr<SlashingManager> slashing_manager, const addr_t& node_addr,
                        const std::string& log_channel_name);

  virtual ~ExtVotesPacketHandler() = default;
  ExtVotesPacketHandler(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler(ExtVotesPacketHandler&&) = delete;
  ExtVotesPacketHandler& operator=(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler& operator=(ExtVotesPacketHandler&&) = delete;

  /**
   * @brief Process vote
   *
   * @param vote
   * @param pbft_block
   * @param peer
   * @param validate_max_round_step
   * @return if vote was successfully processed, otherwise false
   */
  bool processVote(const std::shared_ptr<PbftVote>& vote, const std::shared_ptr<PbftBlock>& pbft_block,
                   const std::shared_ptr<TaraxaPeer>& peer, bool validate_max_round_step);

  /**
   * @brief Checks is vote is relevant for current pbft state in terms of period, round and type
   * @param vote
   * @return true if vote is relevant for current pbft state, otherwise false
   */
  bool isPbftRelevantVote(const std::shared_ptr<PbftVote>& vote) const;

  void requestPbftNextVotesAtPeriodRound(const dev::p2p::NodeID& peerID, PbftPeriod pbft_period, PbftRound pbft_round);

 private:
  /**
   * @brief Validates vote period, round and step against max values from config
   *
   * @param vote to be validated
   * @param peer
   * @param validate_max_round_step validate also max round and step
   * @return <true, ""> vote validation passed, otherwise <false, "err msg">
   */
  std::pair<bool, std::string> validateVotePeriodRoundStep(const std::shared_ptr<PbftVote>& vote,
                                                           const std::shared_ptr<TaraxaPeer>& peer,
                                                           bool validate_max_round_step);

  /**
   * @brief Validates provided vote if voted value == provided block
   *
   * @param vote
   * @param pbft_block
   * @return true if validation successful, otherwise false
   */
  bool validateVoteAndBlock(const std::shared_ptr<PbftVote>& vote, const std::shared_ptr<PbftBlock>& pbft_block) const;

 protected:
  constexpr static size_t kMaxVotesInBundleRlp{1000};
  constexpr static std::chrono::seconds kSyncRequestInterval = std::chrono::seconds(10);

  mutable std::chrono::system_clock::time_point last_votes_sync_request_time_;
  mutable std::chrono::system_clock::time_point last_pbft_block_sync_request_time_;

  std::shared_ptr<PbftManager> pbft_mgr_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<SlashingManager> slashing_manager_;
};

}  // namespace taraxa::network::tarcap
