#pragma once

#include "network/tarcap/capability_latest/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa::network::tarcap::v1 {

/**
 * @brief ExtVotesPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that process pbft votes
 */
class ExtVotesPacketHandler : public tarcap::ExtVotesPacketHandler {
 public:
  // TODO: can be used this instead of ctor copy/paste ?
  // using tarcap::ExtVotesPacketHandler::TaraxaCapabilityBase;
  ExtVotesPacketHandler(const FullNodeConfig& conf, std::shared_ptr<tarcap::PeersState> peers_state,
                        std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<VoteManager> vote_mgr, const addr_t& node_addr,
                        const std::string& log_channel_name);

  virtual ~ExtVotesPacketHandler() = default;
  ExtVotesPacketHandler(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler(ExtVotesPacketHandler&&) = delete;
  ExtVotesPacketHandler& operator=(const ExtVotesPacketHandler&) = delete;
  ExtVotesPacketHandler& operator=(ExtVotesPacketHandler&&) = delete;

  void sendPbftVotesBundle(const std::shared_ptr<tarcap::TaraxaPeer>& peer,
                           std::vector<std::shared_ptr<Vote>>&& votes) override;
};

}  // namespace taraxa::network::tarcap::v1
