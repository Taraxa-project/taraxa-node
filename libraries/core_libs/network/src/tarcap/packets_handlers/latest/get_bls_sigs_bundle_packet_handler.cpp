#include "network/tarcap/packets_handlers/latest/get_bls_sigs_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

GetBlsSigsBundlePacketHandler::GetBlsSigsBundlePacketHandler(
    const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
    std::shared_ptr<TimePeriodPacketsStats> packets_stats,
    std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager, const addr_t &node_addr,
    const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr,
                    logs_prefix + "GET_PILLAR_SIGS_BUNDLE_PH"),
      pillar_chain_manager_(std::move(std::move(pillar_chain_manager))) {}

void GetBlsSigsBundlePacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kGetBlsSigsPacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kGetBlsSigsPacketSize);
  }
}

void GetBlsSigsBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                            const std::shared_ptr<TaraxaPeer> &peer) {
  const PbftPeriod period = packet_data.rlp_[0].toInt();
  const pillar_chain::PillarBlock::Hash pillar_block_hash =
      packet_data.rlp_[1].toHash<pillar_chain::PillarBlock::Hash>();

  const auto signatures = pillar_chain_manager_->getVerifiedBlsSignatures(period, pillar_block_hash);
  if (signatures.empty()) {
    LOG(log_dg_) << "No BLS signatures for period " << period << "and pillar block hash " << pillar_block_hash;
    return;
  }

  // TODO: split packet to multiple with kGetBlsSigsPacketSize sigs containing each
  dev::RLPStream s(signatures.size());
  for (const auto &sig : signatures) {
    s.appendRaw(sig->getRlp());
  }

  if (sealAndSend(peer->getId(), SubprotocolPacketType::BlsSigsBundlePacket, std::move(s))) {
    for (const auto &sig : signatures) {
      peer->markBlsSigAsKnown(sig->getHash());
    }

    LOG(log_dg_) << "BLS signatures bundle sent to " << peer->getId();
  }
}

void GetBlsSigsBundlePacketHandler::requestBlsSigsBundle(PbftPeriod period,
                                                         const pillar_chain::PillarBlock::Hash &pillar_block_hash,
                                                         const std::shared_ptr<TaraxaPeer> &peer) {
  dev::RLPStream s(2);
  s << period;
  s << pillar_block_hash;

  sealAndSend(peer->getId(), SubprotocolPacketType::GetBlsSigsBundlePacket, std::move(s));
  LOG(log_dg_) << "Requested BLS signatures bundle for period " << period << " and pillar block " << pillar_block_hash
               << " from peer " << peer->getId();
}

}  // namespace taraxa::network::tarcap
