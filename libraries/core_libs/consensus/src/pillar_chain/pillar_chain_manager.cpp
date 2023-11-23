#include "pillar_chain/pillar_chain_manager.hpp"

#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "vote_manager/vote_manager.hpp"

// TODO: should ne #include <libBLS/tools/utils.h>
#include <tools/utils.h>

namespace taraxa {

PillarChainManager::PillarChainManager(std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<final_chain::FinalChain> final_chain,
                                       std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<KeyManager> key_manager,
                                       const libff::alt_bn128_Fr& bls_secret_key, addr_t node_addr)
    : db_(std::move(db)),
      network_{},
      final_chain_{std::move(final_chain)},
      vote_mgr_(std::move(vote_mgr)),
      key_manager_(std::move(key_manager)),
      node_addr_(node_addr),
      kBlsSecretKey(bls_secret_key),
      last_pillar_block_{nullptr},
      bls_signatures_{},
      mutex_{} {
  libBLS::ThresholdUtils::initCurve();

  // TODO: load last_pillar_block_ from db

  LOG_OBJECTS_CREATE("PILLAR_CHAIN");
}

void PillarChainManager::createPillarBlock(const std::shared_ptr<final_chain::FinalizationResult>& block_data) {
  const auto block_num = block_data->final_chain_blk->number;

  // There should always be last_pillar_block_, except for the very first pillar block
  assert(block_num <= kEpochBlocksNum || last_pillar_block_);

  // TODO: this will not work for light node
  // Get validators stakes changes between the current and previous pillar block
  auto stakes_changes = getOrderedValidatorsStakesChanges(block_num);

  const auto pillar_block = std::make_shared<PillarBlock>(block_num, block_data->final_chain_blk->state_root,
                                                          std::move(stakes_changes), last_pillar_block_->getHash());
  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    last_pillar_block_ = pillar_block;
    // Erase bls signatures for previous pillar block
    std::erase_if(bls_signatures_, [new_pillar_block_period = pillar_block->getPeriod()](const auto& item) {
      return item.first != new_pillar_block_period;
    });
  }

  // Create and broadcast own bls signature
  // TODO: do not create & gossip own sig or request other signatures if the node is in syncing state

  // Check if node is eligible validator
  try {
    if (!final_chain_->dpos_is_eligible(block_num, node_addr_)) {
      return;
    }
  } catch (state_api::ErrFutureBlock& e) {
    // TODO: true ???
    assert(false);  // This should never happen as newFinalBlock is triggered after the new block is finalized
    return;
  }

  if (!key_manager_->getBlsKey(block_num, node_addr_)) {
    LOG(log_er_) << "No bls public key registered in dpos contract !";
    return;
  }

  // Creates bls signature
  auto signature = std::make_shared<BlsSignature>(pillar_block->getHash(), block_num, node_addr_, kBlsSecretKey);

  // Broadcasts bls signature
  if (validateBlsSignature(signature) && addVerifiedBlsSig(signature)) {
    if (auto net = network_.lock()) {
      net->gossipBlsSignature(signature);
    }
  }
}

void PillarChainManager::checkTwoTPlusOneBlsSignatures(EthBlockNumber block_num) const {
  // Perform the check only every kCheckLatestBlockBlsSigs blocks
  if (block_num % kCheckLatestBlockBlsSigs != 0) {
    return;
  }

  std::shared_ptr<PillarBlock> last_pillar_block{nullptr};
  {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // No last pillar block registered... This should happen only before the first pillar block is created
    if (!last_pillar_block_) [[unlikely]] {
      return;
    }

    // Check 2t+1 signatures
    const auto period_signatures = bls_signatures_.find(last_pillar_block_->getPeriod());
    if (period_signatures == bls_signatures_.end()) [[unlikely]] {
      return;
    }

    const auto pillar_block_signatures =
        period_signatures->second.pillar_block_signatures.find(last_pillar_block_->getHash());
    if (pillar_block_signatures == period_signatures->second.pillar_block_signatures.end()) [[unlikely]] {
      return;
    }

    if (pillar_block_signatures->second.weight >= period_signatures->second.two_t_plus_one) {
      // There is >= 2t+1 bls signatures
      return;
    }

    last_pillar_block = last_pillar_block_;
  }

  // There is < 2t+1 bls signatures, request it
  if (auto net = network_.lock()) {
    net->requestBlsSigBundle(last_pillar_block->getPeriod(), last_pillar_block->getHash());
  }
}

bool PillarChainManager::isRelevantBlsSig(const std::shared_ptr<BlsSignature> signature) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto signature_period = signature->getPeriod();
  // TODO: last_pillar_block_ might be empty
  if (signature_period == last_pillar_block_->getPeriod()) {
    if (signature->getPillarBlockHash() != last_pillar_block_->getHash()) {
      LOG(log_er_) << "Received signature's block hash " << signature->getPillarBlockHash()
                   << " != last pillar block hash " << last_pillar_block_->getHash();
      return false;
    }
  } else if (signature_period != last_pillar_block_->getPeriod() + kEpochBlocksNum /* +1 future pillar block */) {
    LOG(log_er_) << "Received signature's period " << signature_period << ", last pillar block period "
                 << last_pillar_block_->getPeriod();
    return false;
  }

  const auto found_period_signatures = bls_signatures_.find(signature_period);
  if (found_period_signatures == bls_signatures_.end()) {
    return true;
  }

  const auto found_pillar_block_signatures =
      found_period_signatures->second.pillar_block_signatures.find(signature->getPillarBlockHash());
  if (found_pillar_block_signatures == found_period_signatures->second.pillar_block_signatures.end()) {
    return true;
  }

  if (found_pillar_block_signatures->second.signatures.contains(signature->getHash())) {
    return false;
  }

  return true;
}

bool PillarChainManager::isUniqueBlsSig(const std::shared_ptr<BlsSignature> signature) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto found_period_signatures = bls_signatures_.find(signature->getPeriod());
  if (found_period_signatures == bls_signatures_.end()) {
    return true;
  }

  const auto found_signer = found_period_signatures->second.unique_signers.find(signature->getSignerAddr());
  if (found_signer == found_period_signatures->second.unique_signers.end()) {
    return true;
  }

  if (found_signer->second == signature->getHash()) {
    return true;
  }

  LOG(log_er_) << "Non unique bls sig: " << signature->getHash() << ". Existing bls signature " << found_signer->second
               << " for period " << signature->getPeriod() << " and signer " << signature->getSignerAddr();
  return false;
}

bool PillarChainManager::validateBlsSignature(const std::shared_ptr<BlsSignature> signature) const {
  const auto sig_period = signature->getPeriod();
  const auto sig_author = signature->getSignerAddr();

  // Check if signature is unique per period & validator (signer)
  if (!isUniqueBlsSig(signature)) {
    LOG(log_er_) << "Bls Signature " << signature->getHash() << " is not unique per period & validator";
    return false;
  }

  // Check if signer registered his bls key
  const auto bls_pub_key = key_manager_->getBlsKey(sig_period, sig_author);
  if (!bls_pub_key) {
    LOG(log_er_) << "No bls public key mapped for node " << sig_author << ". Signature " << signature->getHash();
    return false;
  }

  // Check if signer is eligible validator
  try {
    if (!final_chain_->dpos_is_eligible(sig_period, sig_author)) {
      LOG(log_er_) << "Signer is not eligible validator. Signature " << signature->getHash();
      return false;
    }
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_wr_) << "Period " << sig_period << " is too far ahead of DPOS. Signature " << signature->getHash()
                 << ". Err: " << e.what();
    return false;
  }

  if (!signature->isValid(bls_pub_key)) {
    LOG(log_er_) << "Invalid signature " << signature->getHash();
    return false;
  }

  return true;
}

bool PillarChainManager::addVerifiedBlsSig(const std::shared_ptr<BlsSignature>& signature) {
  uint64_t signer_vote_count = 0;
  try {
    signer_vote_count = final_chain_->dpos_eligible_vote_count(signature->getPeriod(), signature->getSignerAddr());
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Signature " << signature->getHash() << " period " << signature->getPeriod()
                 << " is too far ahead of DPOS. " << e.what();
    return false;
  }

  if (!signer_vote_count) {
    // Signer is not a valid validator
    LOG(log_er_) << "Signature " << signature->getHash() << " author's " << signature->getSignerAddr()
                 << " stake is zero";
    return false;
  }

  std::vector<libff::alt_bn128_G1> bls_signatures;
  dev::RLPStream signers_addresses_rlp;
  PillarBlock::Hash pillar_block_hash;

  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);

    auto period_signatures = bls_signatures_.find(signature->getPeriod());
    if (period_signatures == bls_signatures_.end()) {
      // Get 2t+1 threshold
      // Note: do not use signature->getPeriod() - 1 as in votes processing - signature are made after the block is
      // finalized, not before as votes
      const auto two_t_plus_one = vote_mgr_->getPbftTwoTPlusOne(signature->getPeriod(), PbftVoteTypes::cert_vote);
      // getPbftTwoTPlusOne returns empty optional only when requested period is too far ahead and that should never
      // happen as this exception is caught above when calling dpos_eligible_vote_count
      assert(two_t_plus_one.has_value());

      period_signatures =
          bls_signatures_.insert({signature->getPeriod(), BlsSignatures{.two_t_plus_one = *two_t_plus_one}}).first;
    }

    auto pillar_block_signatures =
        period_signatures->second.pillar_block_signatures.insert({signature->getPillarBlockHash(), {}}).first;

    if (!pillar_block_signatures->second.signatures.emplace(signature->getHash(), signature).second) {
      // Signature is already saved
      return true;
    }

    pillar_block_signatures->second.weight += signer_vote_count;

    const auto unique_signer =
        period_signatures->second.unique_signers.emplace(signature->getSignerAddr(), signature->getHash());
    if (!unique_signer.second) {
      LOG(log_er_) << "Signer's " << signature->getSignerAddr() << " existing signature " << unique_signer.first->second
                   << ", new signature " << signature->getHash() << " for period " << signature->getPeriod();
      assert(false);
    }

    if (pillar_block_signatures->second.weight < period_signatures->second.two_t_plus_one) {
      // Signatures weight < 2t+1
      return true;
    }

    pillar_block_hash = signature->getPillarBlockHash();

    signers_addresses_rlp.appendList(pillar_block_signatures->second.signatures.size());
    for (const auto& bls_sig : pillar_block_signatures->second.signatures) {
      signers_addresses_rlp << bls_sig.second->getSignerAddr();
      bls_signatures.push_back(bls_sig.second->getSignature());
    }
  }

  // Create aggregated bls signature and save it to db
  libff::alt_bn128_G1 aggregated_signature = libBLS::Bls::Aggregate(bls_signatures);

  std::stringstream aggregated_signature_ss;
  aggregated_signature_ss << aggregated_signature;

  dev::RLPStream bls_aggregated_sig_rlp(3);
  bls_aggregated_sig_rlp << pillar_block_hash;
  bls_aggregated_sig_rlp << aggregated_signature_ss.str();
  bls_aggregated_sig_rlp.appendRaw(signers_addresses_rlp.invalidate());

  // TODO: save bls_aggregated_sig_rlp to db

  return true;
}

std::vector<std::shared_ptr<BlsSignature>> PillarChainManager::getVerifiedBlsSignatures(
    PbftPeriod period, const PillarBlock::Hash pillar_block_hash) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  const auto found_period_signatures = bls_signatures_.find(period);
  if (found_period_signatures == bls_signatures_.end()) {
    return {};
  }

  const auto found_pillar_block_signatures =
      found_period_signatures->second.pillar_block_signatures.find(pillar_block_hash);
  if (found_pillar_block_signatures == found_period_signatures->second.pillar_block_signatures.end()) {
    return {};
  }

  std::vector<std::shared_ptr<BlsSignature>> signatures;
  signatures.reserve(found_pillar_block_signatures->second.signatures.size());
  for (const auto& sig : found_pillar_block_signatures->second.signatures) {
    signatures.push_back(sig.second);
  }

  return signatures;
}

std::vector<PillarBlock::ValidatorStakeChange> PillarChainManager::getOrderedValidatorsStakesChanges(
    EthBlockNumber block) const {
  assert(block % kEpochBlocksNum == 0);

  auto current_stakes = final_chain_->dpos_validators_total_stakes(block);

  // First pillar block - return all current stakes
  if (!last_pillar_block_) {
    std::vector<PillarBlock::ValidatorStakeChange> changes;
    changes.reserve(current_stakes.size());
    std::transform(current_stakes.begin(), current_stakes.end(), std::back_inserter(changes), [](auto& stake) {
      return PillarBlock::ValidatorStakeChange{stake.addr, stake.stake};
    });

    return changes;
  }

  auto transformToMap = [](const std::vector<state_api::ValidatorStake>& changes) {
    std::map<addr_t, PillarBlock::ValidatorStakeChange> changes_map;
    for (const auto& change : changes) {
      // Convert ValidatorStake.stake uint256 to ValidatorStakeChange.stake int256
      changes_map[change.addr] = PillarBlock::ValidatorStakeChange{change.addr, change.stake};
    }

    return changes_map;
  };

  auto previous_stakes = final_chain_->dpos_validators_total_stakes(last_pillar_block_->getPeriod());

  auto current_stakes_map = transformToMap(current_stakes);
  auto previous_stakes_map = transformToMap(previous_stakes);

  // First create ordered map so the changes are ordered by validator addresses
  std::map<addr_t, PillarBlock::ValidatorStakeChange> changes_map;
  for (auto& current_stake : current_stakes_map) {
    auto previous_stake = previous_stakes_map.find(current_stake.first);

    // Previous stakes does not contain validator address from current stakes -> new stake(delegator)
    if (previous_stake == previous_stakes_map.end()) {
      changes_map.emplace(std::move(current_stake));
      continue;
    }

    // Previous stakes contains validator address from current stakes -> substitute the stakes
    changes_map[current_stake.first] = PillarBlock::ValidatorStakeChange{
        current_stake.first, current_stake.second.stake - previous_stake->second.stake};

    // Delete item from previous_stakes - based on left stakes we know which delegators undelegated all tokens
    previous_stakes_map.erase(previous_stake);
  }

  // All previous stakes that were not deleted are delegators who undelegated all of their tokens between current and
  // previous pillar block. Add these stakes as negative numbers into changes
  for (auto& previous_stake_left : previous_stakes_map) {
    previous_stake_left.second.stake *= -1;
    changes_map.emplace(std::move(previous_stake_left));
  }

  // Transform ordered map of changes to vector
  std::vector<PillarBlock::ValidatorStakeChange> changes;
  changes.reserve(changes_map.size());
  std::transform(changes_map.begin(), changes_map.end(), std::back_inserter(changes),
                 [](auto& stake_change) { return std::move(stake_change.second); });

  return changes;
}

void PillarChainManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa
