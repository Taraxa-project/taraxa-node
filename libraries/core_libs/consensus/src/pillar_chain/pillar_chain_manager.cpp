#include "pillar_chain/pillar_chain_manager.hpp"

#include "network/network.hpp"

// TODO: should ne #include <libBLS/tools/utils.h>
#include <tools/utils.h>

namespace taraxa {

PillarChainManager::PillarChainManager(std::shared_ptr<DbStorage> db, addr_t node_addr)
    : db_(std::move(db)),
      network_{},
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
    const auto pillar_block = std::make_shared<PillarBlock>(epoch_blocks_merkle_root, last_pillar_block_->getHash());

    // TODO: should we save it into db ??? Theoretically pillar blocks can be made from saved final blocks ...
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    last_pillar_block_ = pillar_block;
  } else if (block_data.final_chain_blk->number % (kEpochBlocksNum + kBlsSigBroadcastDelayBlocks) == 0) {
    // Create and broadcast bls signature with kBlsSigBroadcastDelayBlocks delay to make sure other up-to-date nodes
    // already processed the latest pillar block
    std::shared_ptr<BlsSignature> signature;
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      signature = std::make_shared<BlsSignature>(last_pillar_block_->getHash(), bls_keys_.second, bls_keys_.first);
    }

    if (auto net = network_.lock()) {
      net->gossipBlsSignature(signature);
    }

  } else if (block_data.final_chain_blk->number % kCheckLatestBlockBlsSigs == 0) {
    // Check if the node has 2t+1 bls signatures. If not, request it
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // TODO: request bls signatures
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
  std::scoped_lock<std::shared_mutex> lock(mutex_);

  if (last_pillar_block_signatures_.emplace(std::make_pair(signature->getHash(), signature)).second) {
    // TODO: adjust also last_pillar_block_signatures_weight_

    return true;
  }

  return false;
}

std::vector<std::shared_ptr<BlsSignature>> PillarChainManager::getVerifiedBlsSigs(const PillarBlock::Hash pillar_block_hash) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  if (pillar_block_hash != last_pillar_block_->getHash()) {
    return {};
  }

  std::vector<std::shared_ptr<BlsSignature>> signatures;
  signatures.reserve(last_pillar_block_signatures_.size());
  for (const auto& sig: last_pillar_block_signatures_) {
    signatures.push_back(sig.second);
  }

  return signatures;
}

void PillarChainManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa
