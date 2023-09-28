#include "pillar_chain/pillar_chain_manager.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"

// TODO: should ne #include <libBLS/tools/utils.h>
#include <tools/utils.h>

namespace taraxa {

PillarChainManager::PillarChainManager(std::shared_ptr<DbStorage> db, std::shared_ptr<final_chain::FinalChain> final_chain,
                                       std::shared_ptr<KeyManager> key_manager, addr_t node_addr)
    : db_(std::move(db)),
      network_{},
      final_chain_{std::move(final_chain)},
      key_manager_(std::move(key_manager)),
      node_addr_(node_addr),
      // TODO: from wallet-config
      bls_keys_(libBLS::Bls::KeyGeneration()),
      // TODO: last_pillar_block_ might be optional ???
      last_pillar_block_{std::make_shared<PillarBlock>(blk_hash_t{0}, blk_hash_t{0})},
      last_pillar_block_signatures_{},
      mutex_{} {
  libBLS::ThresholdUtils::initCurve();

  LOG_OBJECTS_CREATE("PILLAR_CHAIN");
}

void PillarChainManager::newFinalBlock(const final_chain::FinalizationResult& block_data) {
  // TODO: add each final block to final blocks merkle tree
  const auto epoch_blocks_merkle_root = block_data.final_chain_blk->hash;

  // Create pillar block and broadcast BLS signature
  if (block_data.final_chain_blk->number % kEpochBlocksNum == 0) {
    const auto pillar_block = std::make_shared<PillarBlock>(block_data.final_chain_blk->number, epoch_blocks_merkle_root, last_pillar_block_->getHash());

    // TODO: should we save it into db ??? Theoretically pillar blocks can be made from saved final blocks ...
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    last_pillar_block_ = pillar_block;
  } else if (block_data.final_chain_blk->number % (kEpochBlocksNum + kBlsSigBroadcastDelayBlocks) == 0) {
    // Check if node is eligible validator
    try {
      if (!final_chain_->dpos_is_eligible(block_data.final_chain_blk->number, node_addr_)) {
        return;
      }
    } catch (state_api::ErrFutureBlock& e) {
      assert(false); // This should never happen as newFinalBlock is triggered after the new block is finalized
      return;
    }

    if (!key_manager_->getBlsKey(block_data.final_chain_blk->number, node_addr_)) {
      LOG(log_er_) << "No bls public key registered in dpos contract !";
      return;
    }

    // Create and broadcast bls signature with kBlsSigBroadcastDelayBlocks delay to make sure other up-to-date nodes
    // already processed the latest pillar block
    // TODO: maybe dont use the delay and accept signatures with right period ???
    std::shared_ptr<BlsSignature> signature;
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      signature = std::make_shared<BlsSignature>(last_pillar_block_->getHash(), block_data.final_chain_blk->number, node_addr_, bls_keys_.first);
    }

    addVerifiedBlsSig(signature);

    if (auto net = network_.lock()) {
      net->gossipBlsSignature(signature);
    }

    // TODO: fix bls sigs requesting - do it every N blocks after pillar block was created
  } else if (block_data.final_chain_blk->number % kCheckLatestBlockBlsSigs == 0) {
    PillarBlock::Hash last_pillar_block_hash;
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      last_pillar_block_hash = last_pillar_block_->getHash();
    }

    // TODO: Request signature only if node does not have 2t+1 bls signatures
    if (auto net = network_.lock()) {
      net->requestBlsSigBundle(last_pillar_block_hash);
    }
  }
}

bool PillarChainManager::isRelevantBlsSig(const std::shared_ptr<BlsSignature> signature) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  if (last_pillar_block_->getHash() != signature->getPillarBlockHash()) {
    return false;
  }

  if (last_pillar_block_signatures_.contains(signature->getHash())) {
    return false;
  }

  return true;
}

bool PillarChainManager::addVerifiedBlsSig(const std::shared_ptr<BlsSignature>& signature) {
  uint64_t signer_vote_count = 0;
  try {
    signer_vote_count = final_chain_->dpos_eligible_vote_count(signature->getPeriod(), signature->getSignerAddr());
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Signature " << signature->getHash() << " period " << signature->getPeriod() << " is too far ahead of DPOS. " << e.what();
    return false;
  }

  if (!signer_vote_count) {
    LOG(log_er_) << "Signature " << signature->getHash() << " author " << signature->getSignerAddr() << " stake is zero";
    return false;
  }

  std::scoped_lock<std::shared_mutex> lock(mutex_);

  if (last_pillar_block_signatures_.emplace(std::make_pair(signature->getHash(), signature)).second) {
    last_pillar_block_signatures_weight_ += signer_vote_count;
    // TODO: check if we have 2t+1 signatures ???
  }

  return true;
}

std::vector<std::shared_ptr<BlsSignature>> PillarChainManager::getVerifiedBlsSigs(
    const PillarBlock::Hash pillar_block_hash) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  if (pillar_block_hash != last_pillar_block_->getHash()) {
    return {};
  }

  std::vector<std::shared_ptr<BlsSignature>> signatures;
  signatures.reserve(last_pillar_block_signatures_.size());
  for (const auto& sig : last_pillar_block_signatures_) {
    signatures.push_back(sig.second);
  }

  return signatures;
}

void PillarChainManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa
