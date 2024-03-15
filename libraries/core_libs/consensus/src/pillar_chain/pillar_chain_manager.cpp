#include "pillar_chain/pillar_chain_manager.hpp"

#include <libff/common/profiling.hpp>

#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "storage/storage.hpp"
#include "vote/pillar_vote.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa::pillar_chain {

PillarChainManager::PillarChainManager(const FicusHardforkConfig& ficusHfConfig, std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<final_chain::FinalChain> final_chain,
                                       std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<KeyManager> key_manager,
                                       addr_t node_addr)
    : kFicusHfConfig(ficusHfConfig),
      db_(std::move(db)),
      network_{},
      final_chain_{std::move(final_chain)},
      vote_mgr_(std::move(vote_mgr)),
      key_manager_(std::move(key_manager)),
      node_addr_(node_addr),
      last_finalized_pillar_block_{},
      current_pillar_block_{},
      current_pillar_block_stakes_{},
      pillar_votes_{},
      mutex_{} {
  if (const auto vote = db_->getOwnPillarBlockVote(); vote) {
    addVerifiedPillarVote(vote);
  }

  if (auto current_pillar_block = db_->getCurrentPillarBlock(); current_pillar_block) {
    current_pillar_block_ = std::move(current_pillar_block);
  }

  if (auto&& current_pillar_block_stakes = db_->getCurrentPillarBlockStakes(); !current_pillar_block_stakes.empty()) {
    current_pillar_block_stakes_ = std::move(current_pillar_block_stakes);
  }

  if (auto&& latest_pillar_block_data = db_->getLatestPillarBlockData(); latest_pillar_block_data.has_value()) {
    last_finalized_pillar_block_ = std::move(latest_pillar_block_data->block_);
  }

  LOG_OBJECTS_CREATE("PILLAR_CHAIN");
}

std::optional<PillarBlock::Hash> PillarChainManager::createPillarBlock(
    const std::shared_ptr<final_chain::FinalizationResult>& block_data) {
  const auto block_num = block_data->final_chain_blk->number;

  PillarBlock::Hash previous_pillar_block_hash{};  // null block hash
  auto new_stakes = final_chain_->dpos_validators_total_stakes(block_num);
  std::vector<PillarBlock::ValidatorStakeChange> stakes_changes;

  // First ever pillar block
  if (block_num == kFicusHfConfig.firstPillarBlockPeriod()) {
    // First pillar block - use all current stakes as changes
    stakes_changes.reserve(new_stakes.size());
    std::transform(new_stakes.begin(), new_stakes.end(), std::back_inserter(stakes_changes),
                   [](auto& stake) { return PillarBlock::ValidatorStakeChange(stake); });
  } else {
    // Second or further pillar blocks
    if (!current_pillar_block_) {
      LOG(log_er_) << "Empty previous pillar block, new pillar block period " << block_num;
      assert(false);
      return {};
    }

    // Push pillar block into the pillar chain in case it was not finalized yet
    if (!isPillarBlockLatestFinalized(current_pillar_block_->getHash())) {
      // Get 2t+1 verified votes
      auto two_t_plus_one_votes =
          pillar_votes_.getVerifiedVotes(current_pillar_block_->getPeriod(), current_pillar_block_->getHash(), true);
      if (two_t_plus_one_votes.empty()) {
        LOG(log_er_) << "There is < 2t+1 votes for current pillar block " << current_pillar_block_->getHash()
                     << ", period: " << current_pillar_block_->getPeriod() << ". Current period " << block_num;
        return {};
      }

      // Save current pillar block and 2t+1 votes into db
      if (!pushPillarBlock(PillarBlockData{current_pillar_block_, std::move(two_t_plus_one_votes)})) {
        // This should never happen
        LOG(log_er_) << "Unable to push pillar block: " << current_pillar_block_->getHash() << ", period "
                     << current_pillar_block_->getPeriod();
        assert(false);
        return {};
      }
    }

    // This should never happen
    if (current_pillar_block_stakes_.empty()) {
      LOG(log_er_) << "Empty current pillar block stakes, new pillar block period " << block_num;
      assert(false);
      return {};
    }

    previous_pillar_block_hash = current_pillar_block_->getHash();

    // Get validators stakes changes between the current and previous pillar block
    stakes_changes = getOrderedValidatorsStakesChanges(new_stakes, current_pillar_block_stakes_);
  }

  const auto pillar_block = std::make_shared<PillarBlock>(block_num, block_data->final_chain_blk->state_root,
                                                          std::move(stakes_changes), previous_pillar_block_hash);

  // Check if some pillar block was not skipped
  if (!isValidPillarBlock(pillar_block)) {
    LOG(log_er_) << "Newly created pillar block " << pillar_block->getHash() << "with period "
                 << pillar_block->getPeriod() << " is invalid";
    return {};
  }

  LOG(log_nf_) << "New pillar block " << pillar_block->getHash() << " with period " << pillar_block->getPeriod()
               << " created";

  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    current_pillar_block_ = pillar_block;
    current_pillar_block_stakes_ = std::move(new_stakes);

    auto batch = db_->createWriteBatch();
    db_->saveCurrentPillarBlock(current_pillar_block_, batch);
    db_->saveCurrentPillarBlockStakes(current_pillar_block_stakes_, batch);

    // Commit DB
    db_->commitWriteBatch(batch);
  }

  return pillar_block->getHash();
}

bool PillarChainManager::genAndPlacePillarVote(const PillarBlock::Hash& pillar_block_hash, const secret_t& node_sk, bool is_pbft_syncing) {
  std::shared_ptr<PillarVote> vote;
  {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!current_pillar_block_) {
      // This should never happen
      LOG(log_er_) << "Unable to gen pillar vote. No pillar block present, pillar_block_hash " << pillar_block_hash;
      assert(false);
      return false;
    }

    if (!final_chain_->dpos_is_eligible(current_pillar_block_->getPeriod(), toAddress(node_sk))) {
      return false;
    }

    if (pillar_block_hash != current_pillar_block_->getHash()) {
      LOG(log_er_) << "Unable to gen pillar vote. Provided pillar_block_hash(" << pillar_block_hash
                   << ") != current pillar block hash(" << current_pillar_block_->getHash() << ")";
      return false;
    }

    vote = std::make_shared<PillarVote>(node_sk, current_pillar_block_->getPeriod(), current_pillar_block_->getHash());
  }

  // Broadcasts pillar vote
  const auto vote_weight = addVerifiedPillarVote(vote);
  if (!vote_weight) {
    LOG(log_er_) << "Unable to gen pillar vote. Vote was not added to the verified votes. Vote hash "
                 << vote->getHash();
    return false;
  }

  db_->saveOwnPillarBlockVote(vote);
  if (!is_pbft_syncing) {
    if (auto net = network_.lock()) {
      net->gossipPillarBlockVote(vote);
    }

    LOG(log_nf_) << "Placed pillar vote " << vote->getHash() << " for block " << vote->getBlockHash() << ", period "
                 << vote->getPeriod() << ", weight " << vote_weight;
  }

  return true;
}

bool PillarChainManager::pushPillarBlock(const PillarBlockData& pillarBlockData) {
  // Note: 2t+1 votes should be validated before calling pushPillarBlock
  if (!isValidPillarBlock(pillarBlockData.block_)) {
    LOG(log_er_) << "Trying to push invalid pillar block";
    return false;
  }

  db_->savePillarBlockData(pillarBlockData);
  LOG(log_nf_) << "Pillar block " << pillarBlockData.block_->getHash() << " with period "
               << pillarBlockData.block_->getPeriod() << " pushed into the pillar chain";

  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    last_finalized_pillar_block_ = pillarBlockData.block_;

    // Erase votes that are no longer needed
    pillar_votes_.eraseVotes(last_finalized_pillar_block_->getPeriod());
  }

  return true;
}

bool PillarChainManager::isPillarBlockLatestFinalized(const PillarBlock::Hash& block_hash) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  // Current pillar block was already pushed into the pillar chain
  if (last_finalized_pillar_block_ && last_finalized_pillar_block_->getHash() == block_hash) {
    return true;
  }

  return false;
}

std::shared_ptr<PillarBlock> PillarChainManager::getLastFinalizedPillarBlock() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return last_finalized_pillar_block_;
}

std::shared_ptr<PillarBlock> PillarChainManager::getCurrentPillarBlock() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return current_pillar_block_;
}

bool PillarChainManager::checkPillarChainSynced(EthBlockNumber block_num) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  // No current pillar block registered... This should happen only before the first pillar block is created
  if (!current_pillar_block_) [[unlikely]] {
    LOG(log_er_) << "No current pillar block saved, period " << block_num;
    assert(false);
    return false;
  }

  // Check 2t+1 votes for the current pillar block
  if (!pillar_votes_.hasTwoTPlusOneVotes(current_pillar_block_->getPeriod(), current_pillar_block_->getHash())) {
    // There is < 2t+1 pillar votes, request it
    if (auto net = network_.lock()) {
      LOG(log_dg_) << "There is < 2t+1 pillar votes for pillar block " << current_pillar_block_->getHash()
                   << ", period " << current_pillar_block_->getPeriod() << ". Request it";
      net->requestPillarBlockVotesBundle(current_pillar_block_->getPeriod(), current_pillar_block_->getHash());
    } else {
      LOG(log_er_) << "checkPillarChainSynced: Unable to obtain net";
    }

    return false;
  }

  PbftPeriod expected_pillar_block_period = 0;
  if (block_num % kFicusHfConfig.pillar_block_periods == 0) {
    expected_pillar_block_period = block_num - kFicusHfConfig.pillar_block_periods;
  } else {
    expected_pillar_block_period = block_num - (block_num % kFicusHfConfig.pillar_block_periods);
  }

  if (expected_pillar_block_period != current_pillar_block_->getPeriod()) {
    // This should never happen
    LOG(log_er_) << "Pillar chain is out of sync. Current pbft period " << block_num << ", current pillar block period "
                 << current_pillar_block_->getPeriod() << ", expected pillar block period "
                 << expected_pillar_block_period;
    assert(false);
    return false;
  }

  return true;
}

bool PillarChainManager::isRelevantPillarVote(const std::shared_ptr<PillarVote> vote) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  const auto vote_period = vote->getPeriod();
  // Empty current_pillar_block_ means there was no pillar block created yet at all
  if (!current_pillar_block_) [[unlikely]] {
    // It could happen that other nodes created pillar block and voted for it before this node
    if (vote->getPeriod() != kFicusHfConfig.firstPillarBlockPeriod()) {
      LOG(log_nf_) << "Received vote's period " << vote_period << ", no pillar block created yet. Accepting votes with "
                   << kFicusHfConfig.firstPillarBlockPeriod() << " period";
      return false;
    }
  } else if (vote_period == current_pillar_block_->getPeriod()) [[likely]] {
    if (vote->getBlockHash() != current_pillar_block_->getHash()) {
      LOG(log_nf_) << "Received vote's block hash " << vote->getBlockHash() << " != current pillar block hash "
                   << current_pillar_block_->getHash();
      return false;
    }
  } else if (vote_period !=
             current_pillar_block_->getPeriod() + kFicusHfConfig.pillar_block_periods /* +1 future pillar block */) {
    LOG(log_nf_) << "Received vote's period " << vote_period << ", current pillar block period "
                 << current_pillar_block_->getPeriod();
    return false;
  }

  return !pillar_votes_.voteExists(vote);
}

bool PillarChainManager::validatePillarVote(const std::shared_ptr<PillarVote> vote) const {
  const auto period = vote->getPeriod();
  const auto validator = vote->getVoterAddr();

  // Check if vote is unique per period & validator (signer)
  if (!pillar_votes_.isUniqueVote(vote)) {
    LOG(log_er_) << "Pillar vote " << vote->getHash() << " is not unique per period & validator";
    return false;
  }

  // Check if signer is eligible validator
  try {
    if (!final_chain_->dpos_is_eligible(period - 1, validator)) {
      LOG(log_er_) << "Validator is not eligible. Pillar vote " << vote->getHash();
      return false;
    }
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_wr_) << "Period " << period << " is too far ahead of DPOS. Pillar vote " << vote->getHash()
                 << ". Err: " << e.what();
    return false;
  }

  if (!vote->verifyVote()) {
    LOG(log_er_) << "Invalid pillar vote " << vote->getHash();
    return false;
  }

  return true;
}

uint64_t PillarChainManager::addVerifiedPillarVote(const std::shared_ptr<PillarVote>& vote) {
  uint64_t validator_vote_count = 0;
  try {
    validator_vote_count = final_chain_->dpos_eligible_vote_count(vote->getPeriod() - 1, vote->getVoterAddr());
  } catch (state_api::ErrFutureBlock& e) {
    LOG(log_er_) << "Pillar vote " << vote->getHash() << " with period " << vote->getPeriod()
                 << " is too far ahead of DPOS. " << e.what();
    return 0;
  }

  if (!validator_vote_count) {
    LOG(log_er_) << "Zero stake for pillar vote: " << vote->getHash() << ", author: " << vote->getVoterAddr()
                 << ", period: " << vote->getPeriod();
    return 0;
  }

  if (!pillar_votes_.periodDataInitialized(vote->getPeriod())) {
    // Note: do not use vote->getPeriod() - 1 as in votes processing - vote are made after the block is
    // finalized, not before as votes
    if (const auto two_t_plus_one = vote_mgr_->getPbftTwoTPlusOne(vote->getPeriod() - 1, PbftVoteTypes::cert_vote);
        two_t_plus_one.has_value()) {
      pillar_votes_.initializePeriodData(vote->getPeriod(), *two_t_plus_one);
    } else {
      // getPbftTwoTPlusOne returns empty optional only when requested period is too far ahead and that should never
      // happen as this exception is caught above when calling dpos_eligible_vote_count
      LOG(log_er_) << "Unable to get 2t+1 for period " << vote->getPeriod();
      assert(false);
      return 0;
    }
  }

  if (!pillar_votes_.addVerifiedVote(vote, validator_vote_count)) {
    LOG(log_er_) << "Non-unique pillar vote " << vote->getHash() << ", validator " << vote->getVoterAddr();
    return 0;
  }

  LOG(log_nf_) << "Added pillar vote " << vote->getHash();

  // If there is 2t+1 votes for current_pillar_block_, push it to the pillar chain
  const auto current_pillar_block = getCurrentPillarBlock();
  if (current_pillar_block && vote->getBlockHash() == current_pillar_block->getHash() &&
      !isPillarBlockLatestFinalized(current_pillar_block->getHash())) {
    // Get 2t+1 verified votes
    auto two_t_plus_one_votes =
        pillar_votes_.getVerifiedVotes(current_pillar_block->getPeriod(), current_pillar_block->getHash(), true);
    if (!two_t_plus_one_votes.empty()) {
      // Save current pillar block and 2t+1 votes into db
      if (!pushPillarBlock(PillarBlockData{current_pillar_block_, std::move(two_t_plus_one_votes)})) {
        // This should never happen
        LOG(log_er_) << "Unable to push pillar block: " << current_pillar_block->getHash() << ", period "
                     << current_pillar_block->getPeriod();
      }
    }
  }

  return validator_vote_count;
}

std::vector<std::shared_ptr<PillarVote>> PillarChainManager::getVerifiedPillarVotes(
    PbftPeriod period, const PillarBlock::Hash pillar_block_hash) const {
  auto votes = pillar_votes_.getVerifiedVotes(period, pillar_block_hash);

  // No votes returned from memory, try db
  if (votes.empty()) {
    if (auto pillar_block_data = db_->getPillarBlockData(period); pillar_block_data.has_value()) {
      votes = std::move(pillar_block_data->pillar_votes_);
    }
  }

  return votes;
}

bool PillarChainManager::isValidPillarBlock(const std::shared_ptr<PillarBlock>& pillar_block) const {
  // First pillar block
  if (pillar_block->getPeriod() == kFicusHfConfig.firstPillarBlockPeriod()) {
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
    if (const auto stake_change = dev::s256(current_stake.second.stake - previous_stake->second.stake);
        stake_change != 0) {
      changes_map.emplace(current_stake.first, PillarBlock::ValidatorStakeChange(current_stake.first, stake_change));
    }

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
