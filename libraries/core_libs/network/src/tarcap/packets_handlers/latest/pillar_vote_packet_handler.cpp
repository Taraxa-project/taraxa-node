#include "network/tarcap/packets_handlers/latest/pillar_vote_packet_handler.hpp"

#include "vote/pillar_vote.hpp"

namespace taraxa::network::tarcap {

PillarVotePacketHandler::PillarVotePacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                 std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                                                 const addr_t &node_addr, const std::string &logs_prefix)
    : ExtPillarVotePacketHandler(conf, std::move(peers_state), std::move(packets_stats),
                                 std::move(pillar_chain_manager), node_addr, logs_prefix + "PILLAR_VOTE_PH") {}

void PillarVotePacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != PillarVote::kStandardRlpSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, PillarVote::kStandardRlpSize);
  }
}

void PillarVotePacketHandler::process(const threadpool::PacketData &packet_data,
                                      const std::shared_ptr<TaraxaPeer> &peer) {
  const auto pillar_vote = std::make_shared<PillarVote>(packet_data.rlp_);
  if (!kConf.genesis.state.hardforks.ficus_hf.isFicusHardfork(pillar_vote->getPeriod())) {
    std::ostringstream err_msg;
    err_msg << "Pillar vote " << pillar_vote->getHash() << ", period " << pillar_vote->getPeriod()
            << " < ficus hardfork block num";
    throw MaliciousPeerException(err_msg.str());
  }

  processPillarVote(pillar_vote, peer);
  onNewPillarVote(pillar_vote);
}

void PillarVotePacketHandler::onNewPillarVote(const std::shared_ptr<PillarVote> &vote, bool rebroadcast) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      LOG(log_dg_) << "Pillar vote " << vote->getHash() << " not sent to " << peer.first << " peer syncing";
      continue;
    }

    if (peer.second->isPillarVoteKnown(vote->getHash()) && !rebroadcast) {
      continue;
    }

    sendPillarVote(peer.second, vote);
  }
}

void PillarVotePacketHandler::sendPillarVote(const std::shared_ptr<TaraxaPeer> &peer,
                                             const std::shared_ptr<PillarVote> &vote) {
  dev::RLPStream s;
  s.appendRaw(vote->rlp());

  if (sealAndSend(peer->getId(), SubprotocolPacketType::PillarVotePacket, std::move(s))) {
    peer->markPillarVoteAsKnown(vote->getHash());
    LOG(log_nf_) << "Pillar vote " << vote->getHash() << " sent to " << peer->getId();
  }
}

}  // namespace taraxa::network::tarcap
