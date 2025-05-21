#include "network/tarcap/packets_handlers/latest/pillar_vote_packet_handler.hpp"

#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

PillarVotePacketHandler::PillarVotePacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                 std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                                                 const addr_t &node_addr, const std::string &logs_prefix)
    : IPillarVotePacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pillar_chain_manager),
                               node_addr, logs_prefix + "PILLAR_VOTE_PH") {}

void PillarVotePacketHandler::process(const threadpool::PacketData &packet_data,
                                      const std::shared_ptr<TaraxaPeer> &peer) {
  // Decode packet rlp into packet object
  auto packet = decodePacketRlp<PillarVotePacket>(packet_data.rlp_);

  if (!kConf.genesis.state.hardforks.ficus_hf.isFicusHardfork(packet.pillar_vote->getPeriod())) {
    std::ostringstream err_msg;
    err_msg << "Pillar vote " << packet.pillar_vote->getHash() << ", period " << packet.pillar_vote->getPeriod()
            << " < ficus hardfork block num";
    throw MaliciousPeerException(err_msg.str());
  }

  if (processPillarVote(packet.pillar_vote, peer)) {
    onNewPillarVote(packet.pillar_vote);
  }
}

void PillarVotePacketHandler::sendPillarVote(const std::shared_ptr<TaraxaPeer> &peer,
                                             const std::shared_ptr<PillarVote> &vote) {
  if (sealAndSend(peer->getId(), SubprotocolPacketType::kPillarVotePacket, encodePacketRlp(PillarVotePacket(vote)))) {
    peer->markPillarVoteAsKnown(vote->getHash());
    LOG(log_dg_) << "Pillar vote " << vote->getHash() << ", period " << vote->getPeriod() << " sent to "
                 << peer->getId();
  }
}

}  // namespace taraxa::network::tarcap
