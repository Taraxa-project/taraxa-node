#include "network/tarcap/packets_handlers/latest/bls_sigs_bundle_packet_handler.hpp"

namespace taraxa::network::tarcap {

BlsSigsBundlePacketHandler::BlsSigsBundlePacketHandler(const FullNodeConfig &conf,
                                                       std::shared_ptr<PeersState> peers_state,
                                                       std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                                       std::shared_ptr<PillarChainManager> pillar_chain_manager,
                                                       const addr_t &node_addr, const std::string &logs_prefix)
    : ExtBlsSigPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pillar_chain_manager),
                             node_addr, logs_prefix + "BLS_SIGS_BUNDLE_PH") {}

void BlsSigsBundlePacketHandler::validatePacketRlpFormat(
    [[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items == 0 || items > kMaxSignaturesInBundleRlp) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kMaxSignaturesInBundleRlp);
  }
}

void BlsSigsBundlePacketHandler::process(const threadpool::PacketData &packet_data,
                                         const std::shared_ptr<TaraxaPeer> &peer) {
  for (const auto signature_rlp : packet_data.rlp_) {
    const auto bls_signature = std::make_shared<BlsSignature>(signature_rlp);
    processBlsSignature(bls_signature, peer);
  }
}

}  // namespace taraxa::network::tarcap
