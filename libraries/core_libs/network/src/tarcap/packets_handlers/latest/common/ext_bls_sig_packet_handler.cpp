#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "network/tarcap/packets_handlers/latest/common/ext_pillar_vote_packet_handler.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::network::tarcap {

ExtPillarVotePacketHandler::ExtPillarVotePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &log_channel)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, log_channel),
      pillar_chain_manager_{std::move(pillar_chain_manager)} {}

void ExtPillarVotePacketHandler::processPillarVote(const std::shared_ptr<PillarVote> &vote,
                                                   const std::shared_ptr<TaraxaPeer> &peer) {
  if (!pillar_chain_manager_->isRelevantPillarVote(vote)) {
    LOG(log_dg_) << "Drop irrelevant pillar vote " << vote->getHash() << " from peer " << peer->getId();
    return;
  }

  if (!pillar_chain_manager_->validatePillarVote(vote)) {
    std::ostringstream err_msg;
    err_msg << "Invalid pillar vote " << vote->getHash() << " from peer " << peer->getId();
    throw MaliciousPeerException(err_msg.str());
  }

  pillar_chain_manager_->addVerifiedPillarVote(vote);

  // Mark pillar vote as known for peer
  peer->markPillarVoteAsKnown(vote->getHash());
}

}  // namespace taraxa::network::tarcap
