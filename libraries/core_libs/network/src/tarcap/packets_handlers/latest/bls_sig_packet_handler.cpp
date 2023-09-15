#include "network/tarcap/packets_handlers/latest/bls_sig_packet_handler.hpp"

#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::network::tarcap {

BlsSigPacketHandler::BlsSigPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                         std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                         std::shared_ptr<PillarChainManager> pillar_chain_manager,
                                         const addr_t &node_addr, const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, logs_prefix + "BLS_SIG_PH"),
      pillar_chain_manager_{std::move(pillar_chain_manager)} {}

void BlsSigPacketHandler::validatePacketRlpFormat([[maybe_unused]] const threadpool::PacketData &packet_data) const {
  auto items = packet_data.rlp_.itemCount();
  if (items != kBlsSigPacketSize) {
    throw InvalidRlpItemsCountException(packet_data.type_str_, items, kBlsSigPacketSize);
  }
}

void BlsSigPacketHandler::process(const threadpool::PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) {
  const auto bls_signature = std::make_shared<BlsSignature>(packet_data.rlp_[0]);

  if (!pillar_chain_manager_->isRelevantBlsSig(bls_signature)) {
    LOG(log_dg_) << "Drop irrelevant signature " << bls_signature->getHash() << " from peer " << peer->getId();
  }

  if (!bls_signature->isValid()) {
    std::ostringstream err_msg;
    err_msg << "Invalid signature " << bls_signature->getHash() << " from peer " << peer->getId();
    throw MaliciousPeerException(err_msg.str());
  }

  pillar_chain_manager_->addVerifiedBlsSig(bls_signature);

  // Mark bls signature as known for peer
  peer->markBlsSigAsKnown(bls_signature->getHash());
  onNewBlsSig(bls_signature);
}

void BlsSigPacketHandler::onNewBlsSig(const std::shared_ptr<BlsSignature> &signature, bool rebroadcast) {
  for (const auto &peer : peers_state_->getAllPeers()) {
    if (peer.second->syncing_) {
      LOG(log_dg_) << " PBFT vote " << signature << " not sent to " << peer.first << " peer syncing";
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
