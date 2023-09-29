#include "network/tarcap/packets_handlers/latest/common/ext_bls_sig_packet_handler.hpp"

#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"

namespace taraxa::network::tarcap {

ExtBlsSigPacketHandler::ExtBlsSigPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                               std::shared_ptr<PillarChainManager> pillar_chain_manager,
                                               const addr_t &node_addr, const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, logs_prefix + "BLS_SIG_PH"),
      pillar_chain_manager_{std::move(pillar_chain_manager)} {}

void ExtBlsSigPacketHandler::processBlsSignature(const std::shared_ptr<BlsSignature> &signature,
                                                 const std::shared_ptr<TaraxaPeer> &peer) {
  if (!pillar_chain_manager_->isRelevantBlsSig(signature)) {
    LOG(log_dg_) << "Drop irrelevant signature " << signature->getHash() << " from peer " << peer->getId();
    return;
  }

  if (!pillar_chain_manager_->validateBlsSignature(signature)) {
    std::ostringstream err_msg;
    err_msg << "Invalid signature " << signature->getHash() << " from peer " << peer->getId();
    throw MaliciousPeerException(err_msg.str());
  }

  pillar_chain_manager_->addVerifiedBlsSig(signature);

  // Mark bls signature as known for peer
  peer->markBlsSigAsKnown(signature->getHash());
}

}  // namespace taraxa::network::tarcap
