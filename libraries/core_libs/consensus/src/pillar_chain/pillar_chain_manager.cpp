#include "pillar_chain/pillar_chain_manager.hpp"

#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "storage/storage.hpp"
#include "vote_manager/vote_manager.hpp"

// TODO: should ne #include <libBLS/tools/utils.h>
#include <tools/utils.h>

#include <libff/common/profiling.hpp>

namespace taraxa::pillar_chain {

PillarChainManager::PillarChainManager(const FicusHardforkConfig& ficusHfConfig, std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<final_chain::FinalChain> final_chain,
                                       std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<KeyManager> key_manager,
                                       const libff::alt_bn128_Fr& bls_secret_key, addr_t node_addr)
    : kFicusHfConfig(ficusHfConfig),
      db_(std::move(db)),
      network_{},
      final_chain_{std::move(final_chain)},
      vote_mgr_(std::move(vote_mgr)),
      key_manager_(std::move(key_manager)),
      node_addr_(node_addr),
      kBlsSecretKey(bls_secret_key),
      last_finalized_pillar_block_{},
      current_pillar_block_{},
      current_pillar_block_stakes_{},
      signatures_{},
      mutex_{} {
  libBLS::ThresholdUtils::initCurve();
  libff::inhibit_profiling_info = true;  // disable libff (use for bls signatures) internal logging

  if (const auto signature = db_->getOwnLatestBlsSignature(); signature) {
    addVerifiedBlsSig(signature);
  }

  if (auto&& latest_pillar_block_data = db_->getLatestPillarBlockData(); latest_pillar_block_data.has_value()) {
    last_finalized_pillar_block_ = std::move(latest_pillar_block_data->block);
    // TODO: probably dont need this ???
    //    for (const auto& signature : latest_pillar_block_data->signatures) {
    //      addVerifiedBlsSig(signature);
    //    }

    // TODO: get current pillar block from db
    // current_pillar_block_ =

    // TODO: we might need to sve this in db due to light node short history ???
    current_pillar_block_stakes_ = final_chain_->dpos_validators_total_stakes(current_pillar_block_->getPeriod());
  }

  LOG_OBJECTS_CREATE("PILLAR_CHAIN");
}

void PillarChainManager::createPillarBlock(const std::shared_ptr<final_chain::FinalizationResult>& block_data) {
  const auto block_num = block_data->final_chain_blk->number;

  PillarBlock::Hash previous_pillar_block_hash{};  // null block hash
  auto new_stakes = final_chain_->dpos_validators_total_stakes(block_num);
  std::vector<PillarBlock::ValidatorStakeChange> stakes_changes;

  // There should always be latest_pillar_block_, except for the very first pillar block
  if (block_num > kFicusHfConfig.pillar_block_periods) [[likely]] {  // Not the first pillar block epoch
    // TODO: do we need this mutex for latest_pillar_block_, latest_pillar_block_stakes_ and signatures_ ?
    std::shared_lock<std::shared_mutex> lock(mutex_);

    if (!current_pillar_block_) {
      LOG(log_er_) << "Empty previous pillar block, new pillar block period " << block_num;
      assert(false);
      return;
    }

    // Get 2t+1 verified signatures
    const auto two_t_plus_one_signatures = signatures_.getVerifiedBlsSignatures(current_pillar_block_->getPeriod(),
                                                                                current_pillar_block_->getHash(), true);
    if (two_t_plus_one_signatures.empty()) {
      LOG(log_er_) << "There is < 2t+1 signatures for current pillar block " << current_pillar_block_->getHash()
                   << ", period: " << current_pillar_block_->getPeriod();
      return;
    }

    // Save current pillar block and 2t+1 signatures into db
    if (!pushPillarBlock(PillarBlockData{current_pillar_block_, two_t_plus_one_signatures})) {
      // This should never happen
      LOG(log_er_) << "Unable to push pillar block: " << current_pillar_block_->getHash();
      assert(false);
      return;
    }

    // This should never happen
    if (current_pillar_block_stakes_.empty()) {
      LOG(log_er_) << "Empty current pillar block stakes, new pillar block period " << block_num;
      assert(false);
      return;
    }

    previous_pillar_block_hash = current_pillar_block_->getHash();

    // Get validators stakes changes between the current and previous pillar block
    stakes_changes = getOrderedValidatorsStakesChanges(new_stakes, current_pillar_block_stakes_);
  } else {
    // First pillar block - use all current stakes as changes
    stakes_changes.reserve(new_stakes.size());
    std::transform(new_stakes.begin(), new_stakes.end(), std::back_inserter(stakes_changes),
                   [](auto& stake) { return PillarBlock::ValidatorStakeChange(stake); });
  }

  const auto pillar_block = std::make_shared<PillarBlock>(block_num, block_data->final_chain_blk->state_root,
                                                          std::move(stakes_changes), previous_pillar_block_hash);

  // Check if some pillar block was not skipped
  if (!isValidPillarBlock(pillar_block)) {
    LOG(log_er_) << "Newly created pillar block is invalid";
    return;
  }

  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    current_pillar_block_ = pillar_block;
    current_pillar_block_stakes_ = std::move(new_stakes);

    // TODO: save both current_pillar_block_ & current_pillar_block_stakes_ into db ???
  }

  // Create and broadcast own bls signature
  // TODO: do not create & gossip own sig or request other signatures if the node is in syncing state ???

  // Check if node is an eligible validator
  try {
    if (!final_chain_->dpos_is_eligible(block_num, node_addr_)) {
      return;
    }
  } catch (state_api::ErrFutureBlock& e) {
    assert(false);  // This should never happen as newFinalBlock is triggered after the new block is finalized
    return;
  }

  if (!key_manager_->getBlsKey(block_num, node_addr_)) {
    LOG(log_er_) << "No bls public key registered for acc " << node_addr_ << " in dpos contract !";
    return;
  }

  // Creates bls signature
  // TODO: this is not good - what if there won't be consensus on pillar block, butpbft goes on ? We need some sort of
  // repetitive voting
  auto signature = std::make_shared<BlsSignature>(pillar_block->getHash(), block_num, node_addr_, kBlsSecretKey);

  // Broadcasts bls signature
  if (validateBlsSignature(signature) && addVerifiedBlsSig(signature)) {
    db_->saveOwnLatestBlsSignature(signature);

    if (auto net = network_.lock()) {
      net->gossipBlsSignature(signature);
    }
  }
}

bool PillarChainManager::pushPillarBlock(const PillarBlockData& pillarBlockData) {
  // Note: 2t+1 signatures should be validated before calling pushPillarBlock
  if (!isValidPillarBlock(pillarBlockData.block)) {
    LOG(log_er_) << "Trying to push invalid pillar block";
    return false;
  }

  db_->savePillarBlockData(pillarBlockData);

  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    last_finalized_pillar_block_ = pillarBlockData.block;

    // Erase signatures that are no longer needed
    signatures_.eraseSignatures(last_finalized_pillar_block_->getPeriod());
  }

  return true;
}

void PillarChainManager::checkPillarChainSynced(EthBlockNumber block_num) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  // No current pillar block registered... This should happen only before the first pillar block is created
  if (!current_pillar_block_) [[unlikely]] {
    if (block_num > kFicusHfConfig.pillar_block_periods) {
      LOG(log_er_) << "No current pillar block saved, period " << block_num;
      assert(false);
    }

    return;
  }

  // Check if node has all previous pillar blocks
  if ((block_num - (block_num % kFicusHfConfig.pillar_block_periods)) != current_pillar_block_->getPeriod()) {
    // Some pillar blocks are missing - request it
    if (auto net = network_.lock()) {
      net->requestPillarBlocks(current_pillar_block_->getPeriod());
    }
    return;
  }

  // Check 2t+1 signatures for the current pillar block
  if (!signatures_.hasTwoTPlusOneSignatures(current_pillar_block_->getPeriod(), current_pillar_block_->getHash())) {
    // There is < 2t+1 bls signatures, request it
    if (auto net = network_.lock()) {
      net->requestBlsSigBundle(current_pillar_block_->getPeriod(), current_pillar_block_->getHash());
    }
    return;
  }
}

bool PillarChainManager::isRelevantBlsSig(const std::shared_ptr<BlsSignature> signature) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto signature_period = signature->getPeriod();
  // Empty current_pillar_block_ means there was no pillar block created yet at all
  if (!current_pillar_block_) [[unlikely]] {
    LOG(log_nf_) << "Received signature's period " << signature_period
                 << ", no pillar block created yet. Accepting signatures with " << kFicusHfConfig.pillar_block_periods
                 << " period";
    return false;
  } else if (signature_period == current_pillar_block_->getPeriod()) {
    if (signature->getPillarBlockHash() != current_pillar_block_->getHash()) {
      LOG(log_nf_) << "Received signature's block hash " << signature->getPillarBlockHash()
                   << " != current pillar block hash " << current_pillar_block_->getHash();
      return false;
    }
  } else if (signature_period !=
             current_pillar_block_->getPeriod() + kFicusHfConfig.pillar_block_periods /* +1 future pillar block */) {
    LOG(log_nf_) << "Received signature's period " << signature_period << ", current pillar block period "
                 << current_pillar_block_->getPeriod();
    return false;
  }

  return !signatures_.signatureExists(signature);
}

bool PillarChainManager::validateBlsSignature(const std::shared_ptr<BlsSignature> signature) const {
  const auto sig_period = signature->getPeriod();
  const auto sig_author = signature->getSignerAddr();

  // Check if signature is unique per period & validator (signer)
  if (!signatures_.isUniqueBlsSig(signature)) {
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
    LOG(log_er_) << "Zero stake for Signature: " << signature->getHash() << ", author: " << signature->getSignerAddr()
                 << ", period: " << signature->getPeriod();
    return false;
  }

  if (!signatures_.periodDataInitialized(signature->getPeriod())) {
    // Note: do not use signature->getPeriod() - 1 as in votes processing - signature are made after the block is
    // finalized, not before as votes
    if (const auto two_t_plus_one = vote_mgr_->getPbftTwoTPlusOne(signature->getPeriod(), PbftVoteTypes::cert_vote);
        two_t_plus_one.has_value()) {
      signatures_.initializePeriodData(signature->getPeriod(), *two_t_plus_one);
    } else {
      // getPbftTwoTPlusOne returns empty optional only when requested period is too far ahead and that should never
      // happen as this exception is caught above when calling dpos_eligible_vote_count
      LOG(log_er_) << "Unable to get 2t+1 for period " << signature->getPeriod();
      assert(false);
      return false;
    }
  }

  if (!signatures_.addVerifiedSig(signature, signer_vote_count)) {
    LOG(log_er_) << " non-unique signature " << signature->getHash() << ", signer " << signature->getSignerAddr();
    return false;
  }

  return true;
}

std::vector<std::shared_ptr<BlsSignature>> PillarChainManager::getVerifiedBlsSignatures(
    PbftPeriod period, const PillarBlock::Hash pillar_block_hash) const {
  return signatures_.getVerifiedBlsSignatures(period, pillar_block_hash);
}

std::optional<PbftPeriod> PillarChainManager::getLastFinalizedPillarBlockPeriod() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  if (!last_finalized_pillar_block_) {
    return {};
  }

  return last_finalized_pillar_block_->getPeriod();
}

bool PillarChainManager::isValidPillarBlock(const std::shared_ptr<PillarBlock>& pillar_block) const {
  // First pillar block
  if (pillar_block->getPeriod() == kFicusHfConfig.pillar_block_periods) {
    return true;
  }

  std::shared_lock<std::shared_mutex> lock(mutex_);
  assert(last_finalized_pillar_block_);

  // Check if some block was not skipped
  if (pillar_block->getPeriod() - kFicusHfConfig.pillar_block_periods == last_finalized_pillar_block_->getPeriod() &&
      pillar_block->getPreviousBlockHash() == last_finalized_pillar_block_->getHash()) {
    return true;
  }

  LOG(log_er_) << "Invalid pillar block: last finalized pillar bock(period): "
               << last_finalized_pillar_block_->getHash() << "(" << last_finalized_pillar_block_->getPeriod()
               << "), new pillar block: " << pillar_block->getHash() << "(" << pillar_block->getPeriod()
               << "), parent block hash: " << pillar_block->getPreviousBlockHash();
  return false;
}

std::vector<PillarBlock::ValidatorStakeChange> PillarChainManager::getOrderedValidatorsStakesChanges(
    const std::vector<state_api::ValidatorStake>& current_stakes,
    const std::vector<state_api::ValidatorStake>& previous_pillar_block_stakes) {
  auto transformToMap = [](const std::vector<state_api::ValidatorStake>& stakes) {
    std::map<addr_t, state_api::ValidatorStake> stakes_map;
    for (auto&& stake : stakes) {
      stakes_map.emplace(stake.addr, stake);
    }

    return stakes_map;
  };

  assert(!previous_pillar_block_stakes.empty());

  auto current_stakes_map = transformToMap(current_stakes);
  auto previous_stakes_map = transformToMap(previous_pillar_block_stakes);

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
    changes_map.emplace(current_stake.first,
                        PillarBlock::ValidatorStakeChange(
                            current_stake.first, dev::s256(current_stake.second.stake - previous_stake->second.stake)));

    // Delete item from previous_stakes - based on left stakes we know which delegators undelegated all tokens
    previous_stakes_map.erase(previous_stake);
  }

  // All previous stakes that were not deleted are delegators who undelegated all of their tokens between current and
  // previous pillar block. Add these stakes as negative numbers into changes
  for (auto& previous_stake_left : previous_stakes_map) {
    auto stake_change = changes_map.emplace(std::move(previous_stake_left)).first;
    stake_change->second.stake_change_ *= -1;
  }

  // Transform ordered map of changes to vector
  std::vector<PillarBlock::ValidatorStakeChange> changes;
  changes.reserve(changes_map.size());
  std::transform(changes_map.begin(), changes_map.end(), std::back_inserter(changes),
                 [](auto& stake_change) { return std::move(stake_change.second); });

  return changes;
}

void PillarChainManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa::pillar_chain
