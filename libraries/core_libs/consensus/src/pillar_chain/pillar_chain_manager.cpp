#include "pillar_chain/pillar_chain_manager.hpp"

#include <libff/common/profiling.hpp>

#include "config/hardfork.hpp"
#include "final_chain/final_chain.hpp"
#include "key_manager/key_manager.hpp"
#include "network/network.hpp"
#include "storage/storage.hpp"
#include "vote/pillar_vote.hpp"

namespace taraxa::pillar_chain {

PillarChainManager::PillarChainManager(const FicusHardforkConfig& ficus_hf_config, std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<final_chain::FinalChain> final_chain,
                                       std::shared_ptr<KeyManager> key_manager)
    : kFicusHfConfig(ficus_hf_config),
      db_(std::move(db)),
      network_{},
      final_chain_{std::move(final_chain)},
      key_manager_(std::move(key_manager)),
      last_finalized_pillar_block_{},
      current_pillar_block_{},
      current_pillar_block_vote_counts_{},
      pillar_votes_{},
      mutex_{},
      logger_(logger::Logging::get().CreateChannelLogger("PILLAR_CHAIN")) {
  if (const auto vote = db_->getOwnPillarBlockVote(); vote) {
    addVerifiedPillarVote(vote);
  }

  if (auto&& current_pillar_block_data = db_->getCurrentPillarBlockData(); current_pillar_block_data.has_value()) {
    current_pillar_block_ = std::move(current_pillar_block_data->pillar_block);
    current_pillar_block_vote_counts_ = std::move(current_pillar_block_data->vote_counts);
  }

  if (auto&& latest_pillar_block = db_->getLatestPillarBlock(); latest_pillar_block) {
    last_finalized_pillar_block_ = std::move(latest_pillar_block);

    const auto& last_finalized_pillar_block_votes =
        db_->getPeriodPillarVotes(last_finalized_pillar_block_->getPeriod() + 1);
    // There should always be pillar votes stored in period data for finalized pillar block
    assert(!last_finalized_pillar_block_votes.empty());
    for (const auto& pillar_vote : last_finalized_pillar_block_votes) {
      addVerifiedPillarVote(pillar_vote);
    }
  }
}

std::shared_ptr<PillarBlock> PillarChainManager::createPillarBlock(
    PbftPeriod period, const std::shared_ptr<const final_chain::BlockHeader>& block_header, const h256& bridge_root,
    const h256& bridge_epoch) {
  blk_hash_t previous_pillar_block_hash{};  // null block hash
  auto new_vote_counts = final_chain_->dposValidatorsVoteCounts(period);
  std::vector<PillarBlock::ValidatorVoteCountChange> votes_count_changes;

  // First ever pillar block
  if (period == kFicusHfConfig.firstPillarBlockPeriod()) {
    // First pillar block - use all current votes counts as changes
    votes_count_changes.reserve(new_vote_counts.size());
    std::transform(new_vote_counts.begin(), new_vote_counts.end(), std::back_inserter(votes_count_changes),
                   [](const auto& vote_count) {
                     return PillarBlock::ValidatorVoteCountChange(vote_count.addr,
                                                                  static_cast<int32_t>(vote_count.vote_count));
                   });
  } else {
    const auto last_finalized_pillar_block = getLastFinalizedPillarBlock();
    // This should never happen !!!
    if (!last_finalized_pillar_block) {
      logger_->error("Empty last finalized pillar block, new pillar block period {}", period);
      assert(false);
      return nullptr;
    }

    // !!!Note: No need to protect current_pillar_block_vote_counts_ as it is read & written only in this function,
    // which is always called once in a time
    // This should never happen !!!
    if (current_pillar_block_vote_counts_.empty()) {
      logger_->error("Empty current pillar block vote counts, new pillar block period {}", period);
      assert(false);
      return nullptr;
    }

    previous_pillar_block_hash = last_finalized_pillar_block->getHash();

    // Get validators vote counts changes between the current and previous pillar block
    votes_count_changes = getOrderedValidatorsVoteCountsChanges(new_vote_counts, current_pillar_block_vote_counts_);
  }

  const auto pillar_block = std::make_shared<PillarBlock>(period, block_header->state_root, previous_pillar_block_hash,
                                                          bridge_root, bridge_epoch, std::move(votes_count_changes));

  // Check if some pillar block was not skipped
  if (!isValidPillarBlock(pillar_block)) {
    logger_->error("Newly created pillar block {}with period {} is invalid", pillar_block->getHash(),
                   pillar_block->getPeriod());
    return nullptr;
  }

  saveNewPillarBlock(pillar_block, std::move(new_vote_counts));
  logger_->info("New pillar block {} with period {} created", pillar_block->getHash(), pillar_block->getPeriod());

  return pillar_block;
}

void PillarChainManager::saveNewPillarBlock(const std::shared_ptr<PillarBlock>& pillar_block,
                                            std::vector<state_api::ValidatorVoteCount>&& new_vote_counts) {
  std::scoped_lock<std::shared_mutex> lock(mutex_);
  db_->saveCurrentPillarBlockData({pillar_block, new_vote_counts});
  current_pillar_block_ = pillar_block;
  current_pillar_block_vote_counts_ = std::move(new_vote_counts);
}

std::shared_ptr<PillarVote> PillarChainManager::genAndPlacePillarVote(PbftPeriod period,
                                                                      const blk_hash_t& pillar_block_hash,
                                                                      const secret_t& node_sk, bool broadcast_vote) {
  const auto vote = std::make_shared<PillarVote>(node_sk, period, pillar_block_hash);

  // Broadcasts pillar vote
  const auto vote_weight = addVerifiedPillarVote(vote);
  if (!vote_weight) {
    logger_->error("Unable to gen pillar vote. Vote was not added to the verified votes. Vote hash {}",
                   vote->getHash());
    return nullptr;
  }
  db_->saveOwnPillarBlockVote(vote);

  if (auto net = network_.lock(); net && broadcast_vote) {
    net->gossipPillarBlockVote(vote);
    logger_->debug("Placed pillar vote {} for block {}, period , weight {}", vote->getHash(), vote->getBlockHash(),
                   vote->getPeriod(), vote_weight);
  } else {
    logger_->debug(
        "Created pillar vote {} for block {}"
        ", period , weight {}",
        vote->getHash(), vote->getBlockHash(), vote->getPeriod(), vote_weight);
  }

  return vote;
}

std::vector<std::shared_ptr<PillarVote>> PillarChainManager::finalizePillarBlock(const blk_hash_t& pillar_block_hash) {
  // Compare provided pillar block hash to the current pillar block
  const auto current_pillar_block = getCurrentPillarBlock();
  if (!current_pillar_block) {
    // This should never happen
    logger_->error("Cannot finalize pillar block {}. Empty current pillar block", pillar_block_hash);
    return {};
  }

  if (current_pillar_block->getHash() != pillar_block_hash) {
    // This should never happen
    logger_->error("Cannot finalize pillar block {}. Provided pillar block hash {} != current pillar block hash {}",
                   pillar_block_hash, pillar_block_hash, current_pillar_block->getHash());
    return {};
  }

  auto pillar_votes =
      getVerifiedPillarVotes(current_pillar_block->getPeriod() + 1, pillar_block_hash, true /* above_threshold */);
  if (pillar_votes.empty()) {
    logger_->error("Cannot finalize pillar block {}. Not enough pillar votes for pillar block. Request it",
                   pillar_block_hash);
    if (auto net = network_.lock()) {
      net->requestPillarBlockVotesBundle(current_pillar_block->getPeriod() + 1, pillar_block_hash);
    }

    return {};
  }

  if (isPillarBlockLatestFinalized(pillar_block_hash)) {
    // This should never happen
    logger_->error("Pillar block already {} already finalized", pillar_block_hash);
    return pillar_votes;
  }

  db_->savePillarBlock(current_pillar_block);
  logger_->info("Pillar block {} with period {} finalized", pillar_block_hash, current_pillar_block->getPeriod());

  {
    std::scoped_lock<std::shared_mutex> lock(mutex_);
    last_finalized_pillar_block_ = current_pillar_block;

    // Erase votes that are no longer needed
    pillar_votes_.eraseVotes(last_finalized_pillar_block_->getPeriod() + 1);
  }
  pillar_block_finalized_emitter_.emit(PillarBlockData{current_pillar_block, pillar_votes});

  return pillar_votes;
}

bool PillarChainManager::isPillarBlockLatestFinalized(const blk_hash_t& block_hash) const {
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

bool PillarChainManager::isRelevantPillarVote(const std::shared_ptr<PillarVote> vote) const {
  const auto current_pillar_block = getCurrentPillarBlock();

  const auto vote_period = vote->getPeriod();
  // Empty current_pillar_block_ means there was no pillar block created yet at all
  if (!current_pillar_block) [[unlikely]] {
    // It could happen that other nodes created pillar block and voted for it before this node
    if (vote->getPeriod() != kFicusHfConfig.firstPillarBlockPeriod() + 1) {
      logger_->info("Received vote's period {}, no pillar block created yet. Accepting votes with {} period",
                    vote_period, kFicusHfConfig.firstPillarBlockPeriod() + 1);
      return false;
    }
  } else if (vote_period == current_pillar_block->getPeriod() + 1) [[likely]] {
    if (vote->getBlockHash() != current_pillar_block->getHash()) {
      logger_->info("Received vote's block hash {} != current pillar block hash {}", vote->getBlockHash(),
                    current_pillar_block->getHash());
      return false;
    }
  } else if (vote_period != current_pillar_block->getPeriod() + kFicusHfConfig.pillar_blocks_interval +
                                1 /* future pillar block votes */) {
    logger_->info("Received vote's period {}, current pillar block period {}", vote_period,
                  current_pillar_block->getPeriod());
    return false;
  }

  if (pillar_votes_.voteExists(vote)) {
    logger_->debug("Received vote {} already saved", vote->getHash());
    return false;
  }

  return true;
}

bool PillarChainManager::validatePillarVote(const std::shared_ptr<PillarVote> vote) const {
  const auto period = vote->getPeriod();
  const auto validator = vote->getVoterAddr();

  // Check if vote is unique per period & validator (signer)
  if (!pillar_votes_.isUniqueVote(vote)) {
    logger_->error("Pillar vote {} is not unique per period & validator", vote->getHash());
    return false;
  }

  // Check if signer is eligible validator
  try {
    if (!final_chain_->dposIsEligible(period - 1, validator)) {
      logger_->error("Validator is not eligible. Pillar vote {}", vote->getHash());
      return false;
    }
  } catch (state_api::ErrFutureBlock& e) {
    logger_->warn("Period {} is too far ahead of DPOS. Pillar vote {}. Err: {}", period, vote->getHash(), e.what());
    return false;
  }

  if (!vote->verifyVote()) {
    logger_->error("Invalid pillar vote {}", vote->getHash());
    return false;
  }

  return true;
}

uint64_t PillarChainManager::addVerifiedPillarVote(const std::shared_ptr<PillarVote>& vote) {
  uint64_t validator_vote_count = 0;
  try {
    validator_vote_count = final_chain_->dposEligibleVoteCount(vote->getPeriod() - 1, vote->getVoterAddr());
  } catch (state_api::ErrFutureBlock& e) {
    logger_->error("Pillar vote {} with period {} is too far ahead of DPOS. {}", vote->getHash(), vote->getPeriod(),
                   e.what());
    return 0;
  }

  if (!validator_vote_count) {
    logger_->error(
        "Zero vote count for pillar vote: {}, author: {}"
        ", period: {}",
        vote->getHash(), vote->getVoterAddr(), vote->getPeriod());
    return 0;
  }

  if (!pillar_votes_.periodDataInitialized(vote->getPeriod())) {
    const auto threshold = getPillarConsensusThreshold(vote->getPeriod() - 1);
    if (!threshold) {
      logger_->error("Unable to get pillar consensus threshold for period {}", vote->getPeriod() - 1);
      return 0;
    }
    pillar_votes_.initializePeriodData(vote->getPeriod(), *threshold);
  }

  if (!pillar_votes_.addVerifiedVote(vote, validator_vote_count)) {
    logger_->error("Non-unique pillar vote {}, period {}, validator {}", vote->getHash(), vote->getPeriod(),
                   vote->getVoterAddr());
    return 0;
  }

  logger_->info("Added pillar vote {}, period {}, pillar block hash {}", vote->getHash(), vote->getPeriod(),
                vote->getBlockHash());

  return validator_vote_count;
}

std::vector<std::shared_ptr<PillarVote>> PillarChainManager::getVerifiedPillarVotes(PbftPeriod period,
                                                                                    const blk_hash_t pillar_block_hash,
                                                                                    bool above_threshold) const {
  auto pillar_votes = pillar_votes_.getVerifiedVotes(period, pillar_block_hash, above_threshold);

  // No votes returned from memory, try db
  if (pillar_votes.empty()) {
    pillar_votes = db_->getPeriodPillarVotes(period);
  }

  return pillar_votes;
}

bool PillarChainManager::isValidPillarBlock(const std::shared_ptr<PillarBlock>& pillar_block) const {
  // First pillar block
  if (pillar_block->getPeriod() == kFicusHfConfig.firstPillarBlockPeriod()) {
    return true;
  }

  const auto last_finalized_pillar_block = getLastFinalizedPillarBlock();
  assert(last_finalized_pillar_block);

  // Check if some block was not skipped
  if (pillar_block->getPeriod() - kFicusHfConfig.pillar_blocks_interval == last_finalized_pillar_block->getPeriod() &&
      pillar_block->getPreviousBlockHash() == last_finalized_pillar_block->getHash()) {
    return true;
  }

  logger_->error(
      "Invalid pillar block: last finalized pillar bock(period): {}({}), new pillar block: {}({}), parent block hash: "
      "{}",
      last_finalized_pillar_block->getHash(), last_finalized_pillar_block->getPeriod(), pillar_block->getHash(),
      pillar_block->getPeriod(), pillar_block->getPreviousBlockHash());
  return false;
}

std::optional<uint64_t> PillarChainManager::getPillarConsensusThreshold(PbftPeriod period) const {
  std::optional<uint64_t> threshold;

  try {
    // Pillar chain consensus threshold = total votes count / 2 + 1
    threshold = final_chain_->dposEligibleTotalVoteCount(period) / 2 + 1;
  } catch (state_api::ErrFutureBlock& e) {
    logger_->error("Unable to get dpos total votes count for period {} to calculate pillar consensus threshold: {}",
                   period, e.what());
  }

  return threshold;
}

std::vector<PillarBlock::ValidatorVoteCountChange> PillarChainManager::getOrderedValidatorsVoteCountsChanges(
    const std::vector<state_api::ValidatorVoteCount>& current_vote_counts,
    const std::vector<state_api::ValidatorVoteCount>& previous_pillar_block_vote_counts) {
  auto transformToMap = [](const std::vector<state_api::ValidatorVoteCount>& vote_counts) {
    std::unordered_map<addr_t, state_api::ValidatorVoteCount> vote_counts_map;
    for (auto&& vote_count : vote_counts) {
      vote_counts_map.emplace(vote_count.addr, vote_count);
    }

    return vote_counts_map;
  };

  assert(!previous_pillar_block_vote_counts.empty());

  auto current_vote_counts_map = transformToMap(current_vote_counts);
  auto previous_vote_counts_map = transformToMap(previous_pillar_block_vote_counts);

  // First create ordered map so the changes are ordered by validator addresses
  std::map<addr_t, PillarBlock::ValidatorVoteCountChange> changes_map;
  for (auto& current_vote_count : current_vote_counts_map) {
    auto previous_vote_count = previous_vote_counts_map.find(current_vote_count.first);

    // Previous vote counts does not contain validator address from current vote counts -> new vote count(delegator)
    if (previous_vote_count == previous_vote_counts_map.end()) {
      changes_map.emplace(
          current_vote_count.second.addr,
          PillarBlock::ValidatorVoteCountChange(current_vote_count.second.addr,
                                                static_cast<int32_t>(current_vote_count.second.vote_count)));
      continue;
    }

    // Previous vote counts contains validator address from current vote counts -> substitute the vote counts
    if (const auto vote_count_change =
            static_cast<int64_t>(current_vote_count.second.vote_count - previous_vote_count->second.vote_count);
        vote_count_change != 0) {
      changes_map.emplace(current_vote_count.first,
                          PillarBlock::ValidatorVoteCountChange(current_vote_count.first, vote_count_change));
    }

    // Delete item from previous_vote_counts - based on left vote counts we know which delegators undelegated all tokens
    previous_vote_counts_map.erase(previous_vote_count);
  }

  // All previous vote counts that were not deleted are delegators who undelegated all of their tokens between current
  // and previous pillar block. Add these vote counts as negative numbers into changes
  for (auto& previous_vote_count_left : previous_vote_counts_map) {
    auto vote_count_change = changes_map
                                 .emplace(previous_vote_count_left.second.addr,
                                          PillarBlock::ValidatorVoteCountChange(
                                              previous_vote_count_left.second.addr,
                                              static_cast<int32_t>(previous_vote_count_left.second.vote_count)))
                                 .first;
    vote_count_change->second.vote_count_change_ *= -1;
  }

  // Transform ordered map of changes to vector
  std::vector<PillarBlock::ValidatorVoteCountChange> changes;
  changes.reserve(changes_map.size());
  std::transform(std::make_move_iterator(changes_map.begin()), std::make_move_iterator(changes_map.end()),
                 std::back_inserter(changes),
                 [](auto&& vote_count_change) { return std::move(vote_count_change.second); });

  return changes;
}

void PillarChainManager::setNetwork(std::weak_ptr<Network> network) { network_ = std::move(network); }

}  // namespace taraxa::pillar_chain
