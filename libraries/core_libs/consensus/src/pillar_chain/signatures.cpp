#include "pillar_chain/signatures.hpp"

namespace taraxa::pillar_chain {

bool Signatures::signatureExists(const std::shared_ptr<BlsSignature> signature) const {
  const auto found_period_signatures = signatures_.find(signature->getPeriod());
  if (found_period_signatures == signatures_.end()) {
    return false;
  }

  const auto found_pillar_block_signatures =
      found_period_signatures->second.pillar_block_signatures.find(signature->getPillarBlockHash());
  if (found_pillar_block_signatures == found_period_signatures->second.pillar_block_signatures.end()) {
    return false;
  }

  return found_pillar_block_signatures->second.signatures.contains(signature->getHash());
}

bool Signatures::isUniqueBlsSig(const std::shared_ptr<BlsSignature> signature) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto found_period_signatures = signatures_.find(signature->getPeriod());
  if (found_period_signatures == signatures_.end()) {
    return true;
  }

  const auto found_signer = found_period_signatures->second.unique_signers.find(signature->getSignerAddr());
  if (found_signer == found_period_signatures->second.unique_signers.end()) {
    return true;
  }

  if (found_signer->second == signature->getHash()) {
    return true;
  }

  return false;
}

bool Signatures::hasTwoTPlusOneSignatures(PbftPeriod period, PillarBlock::Hash block_hash) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  // Check 2t+1 signatures
  const auto found_period_signatures = signatures_.find(period);
  if (found_period_signatures == signatures_.end()) [[unlikely]] {
    return false;
  }

  const auto found_pillar_block_signatures = found_period_signatures->second.pillar_block_signatures.find(block_hash);
  if (found_pillar_block_signatures == found_period_signatures->second.pillar_block_signatures.end()) [[unlikely]] {
    return false;
  }

  if (found_pillar_block_signatures->second.weight < found_period_signatures->second.two_t_plus_one) {
    return false;
  }

  // There is >= 2t+1 bls signatures
  return true;
}

std::vector<std::shared_ptr<BlsSignature>> Signatures::getVerifiedBlsSignatures(
    PbftPeriod period, const PillarBlock::Hash pillar_block_hash, bool two_t_plus_one) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  const auto found_period_signatures = signatures_.find(period);
  if (found_period_signatures == signatures_.end()) {
    return {};
  }

  const auto found_pillar_block_signatures =
      found_period_signatures->second.pillar_block_signatures.find(pillar_block_hash);
  if (found_pillar_block_signatures == found_period_signatures->second.pillar_block_signatures.end()) {
    return {};
  }

  if (two_t_plus_one && found_pillar_block_signatures->second.weight < found_period_signatures->second.two_t_plus_one) {
    return {};
  }

  std::vector<std::shared_ptr<BlsSignature>> signatures;
  signatures.reserve(found_pillar_block_signatures->second.signatures.size());
  for (const auto& sig : found_pillar_block_signatures->second.signatures) {
    signatures.push_back(sig.second);
  }

  return signatures;
}

bool Signatures::periodDataInitialized(PbftPeriod period) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return signatures_.contains(period);
}

bool Signatures::addVerifiedSig(const std::shared_ptr<BlsSignature>& signature, u_int64_t signer_vote_count) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);

  auto found_period_signatures = signatures_.find(signature->getPeriod());
  if (found_period_signatures == signatures_.end()) {
    // Period must be initialized explicitly providing also 2t+1 weight before adding any signature
    assert(false);
    return false;
  }

  if (const auto unique_signer =
          found_period_signatures->second.unique_signers.emplace(signature->getSignerAddr(), signature->getHash());
      !unique_signer.second) {
    if (unique_signer.first->second != signature->getHash()) {
      // Non unique signature for the signer
      return false;
    }
  }

  auto pillar_block_signatures =
      found_period_signatures->second.pillar_block_signatures.insert({signature->getPillarBlockHash(), {}}).first;

  // Add signer vote count only if the signature is new
  if (pillar_block_signatures->second.signatures.emplace(signature->getHash(), signature).second) {
    pillar_block_signatures->second.weight += signer_vote_count;
  }

  return true;
}

void Signatures::initializePeriodData(PbftPeriod period, uint64_t period_two_t_plus_one) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  signatures_.insert({period, PeriodSignatures{.two_t_plus_one = period_two_t_plus_one}});
}

void Signatures::eraseSignatures(PbftPeriod min_period) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  std::erase_if(signatures_, [min_period](const auto& item) { return item.first < min_period; });
}

}  // namespace taraxa::pillar_chain