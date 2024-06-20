#include "network/tarcap/packets_handlers/latest/get_pillar_votes_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

GetPillarVotesBundlePacketHandler::GetPillarVotesBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr,
                    logs_prefix + "GET_PILLAR_VOTES_BUNDLE_PH"),
      pillar_chain_manager_(std::move(pillar_chain_manager)) {}

void GetPillarVotesBundlePacketHandler::validatePacketRlpFormat(const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kGetPillarVotesBundlePacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kGetPillarVotesBundlePacketSize);
  }
}

void GetPillarVotesBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                                const std::shared_ptr<TaraxaPeer> &peer) {
  LOG(log_dg_) << "GetPillarVotesBundlePacketHandler received from peer " << peer->getId();
  const PbftPeriod period = packet_data.rlp_[0].toInt();
  const blk_hash_t pillar_block_hash = packet_data.rlp_[1].toHash<blk_hash_t>();

  if (!kConf.genesis.state.hardforks.ficus_hf.isFicusHardfork(period)) {
    std::ostringstream err_msg;
    err_msg << "Pillar votes bundle request for period " << period << ", ficus hardfork block num "
            << kConf.genesis.state.hardforks.ficus_hf.block_num;
    throw MaliciousPeerException(err_msg.str());
  }

  if (!kConf.genesis.state.hardforks.ficus_hf.isPillarBlockPeriod(period)) {
    std::ostringstream err_msg;
    err_msg << "Pillar votes bundle request for period " << period << ". Wrong requested period";
    throw MaliciousPeerException(err_msg.str());
  }

  const auto votes = pillar_chain_manager_->getVerifiedPillarVotes(period, pillar_block_hash);
  if (votes.empty()) {
    LOG(log_dg_) << "No pillar votes for period " << period << "and pillar block hash " << pillar_block_hash;
    return;
  }

  dev::RLPStream s(votes.size());
  for (const auto &sig : votes) {
    s.appendRaw(sig->rlp());
  }

  if (sealAndSend(peer->getId(), SubprotocolPacketType::PillarVotesBundlePacket, std::move(s))) {
    for (const auto &vote : votes) {
      peer->markPillarVoteAsKnown(vote->getHash());
    }

    LOG(log_nf_) << "Pillar votes bundle sent to " << peer->getId();
  }
}

void GetPillarVotesBundlePacketHandler::requestPillarVotesBundle(PbftPeriod period, const blk_hash_t &pillar_block_hash,
                                                                 const std::shared_ptr<TaraxaPeer> &peer) {
  dev::RLPStream s(kGetPillarVotesBundlePacketSize);
  s << period;
  s << pillar_block_hash;

  if (sealAndSend(peer->getId(), SubprotocolPacketType::GetPillarVotesBundlePacket, std::move(s))) {
    LOG(log_nf_) << "Requested pillar votes bundle for period " << period << " and pillar block " << pillar_block_hash
                 << " from peer " << peer->getId();
  } else {
    LOG(log_er_) << "Unable to send pillar votes bundle request for period " << period << " and pillar block "
                 << pillar_block_hash << " to peer " << peer->getId();
  }
}

}  // namespace taraxa::network::tarcap
