#include "network/tarcap/packets_handlers/latest/common/ext_bls_sig_packet_handler.hpp"
#include "key_manager/key_manager.hpp"
#include "pillar_chain/pillar_chain_manager.hpp"
#include "final_chain/final_chain.hpp"

namespace taraxa::network::tarcap {

ExtBlsSigPacketHandler::ExtBlsSigPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                                               std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                               std::shared_ptr<PillarChainManager> pillar_chain_manager,
                                               std::shared_ptr<KeyManager> key_manager, std::shared_ptr<final_chain::FinalChain> final_chain,
                                               const addr_t &node_addr, const std::string &logs_prefix)
    : PacketHandler(conf, std::move(peers_state), std::move(packets_stats), node_addr, logs_prefix + "BLS_SIG_PH"),
      pillar_chain_manager_{std::move(pillar_chain_manager)},
      key_manager_(std::move(key_manager)),
      final_chain_(std::move(final_chain)){}

void ExtBlsSigPacketHandler::processBlsSignature(const std::shared_ptr<BlsSignature> &signature,
                                                 const std::shared_ptr<TaraxaPeer> &peer) {
  if (!pillar_chain_manager_->isRelevantBlsSig(signature)) {
    LOG(log_dg_) << "Drop irrelevant signature " << signature->getHash() << " from peer " << peer->getId();
    return;
  }

  const auto sig_period = signature->getPeriod();
  const auto sig_author = signature->getSignerAddr();

  // Check if signer registered his bls key
  const auto bls_pub_key = key_manager_->getBlsKey(sig_period, sig_author);
  if (!bls_pub_key) {
    LOG(log_er_) << "No bls public key mapped for node " << sig_author << ". Signature " << signature->getHash();
    return;
  }

  // Check if signer is eligible validator
  try {
    if (!final_chain_->dpos_is_eligible(sig_period, sig_author)) {
      std::ostringstream err_msg;
      err_msg << "Invalid signature " << signature->getHash() << " from peer " << peer->getId() << ". Signer is not eligible validator";
      throw MaliciousPeerException(err_msg.str());
    }
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_wr_) << "Signature " << signature->getHash() << " period " << sig_period << " is too far ahead of DPOS. " << e.what();
    return;
  }

  if (!signature->isValid(bls_pub_key)) {
    std::ostringstream err_msg;
    err_msg << "Invalid signature " << signature->getHash() << " from peer " << peer->getId();
    throw MaliciousPeerException(err_msg.str());
  }

  pillar_chain_manager_->addVerifiedBlsSig(signature);

  // Mark bls signature as known for peer
  peer->markBlsSigAsKnown(signature->getHash());
}

}  // namespace taraxa::network::tarcap
