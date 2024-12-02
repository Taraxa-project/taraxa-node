#pragma once

#include "network/tarcap/packets_handlers/latest/common/ext_votes_packet_handler.hpp"

namespace taraxa::network::tarcap {

class IVotePacketHandler : public ExtVotesPacketHandler {
 public:
  IVotePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                     std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                     std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                     std::shared_ptr<SlashingManager> slashing_manager, const addr_t& node_addr,
                     const std::string& logs_prefix);

  /**
   * @brief Sends pbft vote to connected peers
   *
   * @param vote Votes to send
   * @param block block to send - nullptr means no block
   * @param rebroadcast - send even of vote i known for the peer
   */
  void onNewPbftVote(const std::shared_ptr<PbftVote>& vote, const std::shared_ptr<PbftBlock>& block,
                     bool rebroadcast = false);

  /**
   * @brief Sends pbft vote to specified peer
   * @param peer
   * @param vote
   * @param block
   */
  virtual void sendPbftVote(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<PbftVote>& vote,
                            const std::shared_ptr<PbftBlock>& block);

  /**
   * @brief Sends pbft votes bundle to connected peers
   *
   * @param votes Votes to send
   * @param rebroadcast if rebroadcast is true, all votes are resent to all peers
   * @param exclude_node do not send votes to excluded node
   */
  void onNewPbftVotesBundle(const std::vector<std::shared_ptr<PbftVote>>& votes, bool rebroadcast = false,
                            const std::optional<dev::p2p::NodeID>& exclude_node = {});

  /**
   * @brief Sends pbft votes bundle to specified peer
   * @param peer
   * @param vote
   * @param block
   */
  virtual void sendPbftVotesBundle(const std::shared_ptr<TaraxaPeer>& peer,
                                   std::vector<std::shared_ptr<PbftVote>>&& votes);
};

}  // namespace taraxa::network::tarcap
