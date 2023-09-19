#include "network/tarcap/packets_handlers/latest/bls_sig_packet_handler.hpp"

namespace taraxa::network::tarcap {

BlsSigPacketHandler::BlsSigPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                         std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                         std::shared_ptr<PillarChainManager> pillar_chain_manager,
                                         const addr_t &node_addr, const std::string &logs_prefix)
    : ExtBlsSigPacketHandler(conf, std::move(peers_state), std::move(packets_stats), std::move(pillar_chain_manager),
                             node_addr, logs_prefix + "BLS_SIG_PH") {}

void BlsSigPacketHandler::validatePacketRlpFormat([[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kBlsSigPacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kBlsSigPacketSize);
  }
}

void BlsSigPacketHandler::process(const threadpool::PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto bls_signature = std::make_shared<BlsSignature>(packet_data.rlp_);
  processBlsSignature(bls_signature, peer);
  onNewBlsSig(bls_signature);
}

void BlsSigPacketHandler::onNewBlsSig(const std::shared_ptr<BlsSignature> &signature, bool rebroadcast) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      LOG(log_dg_) << "BLS signature " << signature->getHash() << " not sent to " << peer.first << " peer syncing";
      continue;
    }

    if (peer.second->isBlsSigKnown(signature->getHash()) && !rebroadcast) {
      continue;
    }

    sendBlsSig(peer.second, signature);
  }
}

void BlsSigPacketHandler::sendBlsSig(const std::shared_ptr<TaraxaPeer> &peer,
                                     const std::shared_ptr<BlsSignature> &signature) {
  dev::RLPStream s(1);
  s.appendRaw(signature->getRlp());

  if (sealAndSend(peer->getId(), SubprotocolPacketType::BlsSigPacket, std::move(s))) {
    peer->markBlsSigAsKnown(signature->getHash());
    LOG(log_dg_) << "BLS signature " << signature->getHash() << " sent to " << peer->getId();
  }
}

}  // namespace taraxa::network::tarcap
