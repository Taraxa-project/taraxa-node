#include "votes/rewards_votes.hpp"

#include <mutex>

#include "storage/storage.hpp"
#include "vote/vote.hpp"

namespace taraxa {

RewardsVotes::RewardsVotes(std::shared_ptr<DbStorage> db, uint64_t last_saved_pbft_period) : db_(std::move(db)) {
  // Inits votes containers from database
  new_processed_votes_ = db_->getNewRewardsVotes();

  uint64_t start_period = std::max(uint64_t(0), last_saved_pbft_period - k_max_cached_periods_count) + 1;

  for (uint64_t period = start_period; period <= last_saved_pbft_period; period++) {
    auto cert_votes = db_->getCertVotes(period);
    assert(!cert_votes.empty());

    std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> votes_map;
    for (auto cert_vote : cert_votes) {
      auto vote_hash = cert_vote->getHash();
      votes_map.emplace(std::move(vote_hash), std::move(cert_vote));
    }

    auto voted_block_hash = cert_votes[0]->getBlockHash();
    blocks_cert_votes_ordering_.push(voted_block_hash);
    blocks_cert_votes_.emplace(std::move(voted_block_hash), std::move(votes_map));
  }
}

void RewardsVotes::newPbftBlockFinalized(std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>&& cert_votes,
                                         const blk_hash_t& voted_block_hash,
                                         const std::unordered_set<vote_hash_t>& pbft_rewards_votes) {
  std::unordered_set<vote_hash_t> cert_votes_hashes;
  cert_votes_hashes.reserve(cert_votes.size());
  for (const auto& cert_vote : cert_votes) {
    cert_votes_hashes.insert(cert_vote.first);
  }

  // Save new finalized pbft block 2t+1 cert votes + shift older cached cert votes
  {
    std::scoped_lock lock(blocks_cert_votes_mutex_);
    assert(!blocks_cert_votes_.contains(voted_block_hash));
    assert(blocks_cert_votes_ordering_.size() == blocks_cert_votes_.size());

    blocks_cert_votes_.emplace(voted_block_hash, std::move(cert_votes));
    blocks_cert_votes_ordering_.push(voted_block_hash);

    // Keep only k_max_cached_periods_count cached periods
    if (blocks_cert_votes_.size() > k_max_cached_periods_count) {
      const auto& block_to_be_removed = blocks_cert_votes_ordering_.front();
      blocks_cert_votes_.erase(block_to_be_removed);
      blocks_cert_votes_ordering_.pop();
    }
  }

  // Merge unrewarded_votes_ with new cert_votes_hashes
  {
    std::scoped_lock lock(unrewarded_votes_mutex_);
    unrewarded_votes_.merge(std::move(cert_votes_hashes));
  }

  // Filters dag block rewards votes that are new & included in dag blocks (not in observed 2t+1 cert votes or new & not
  // yet includes in dag blocks)
  const auto new_pbft_rewards_votes = filterNewProcessedVotes(pbft_rewards_votes);

  auto batch = db_->createWriteBatch();

  // Removes new_pbft_rewards_votes from new_rewards_votes db column
  db_->removeNewRewardsVotesToBatch(new_pbft_rewards_votes, batch);

  // Sort new_pbft_rewards_votes by voted pbft blocks
  std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>> new_rewards_votes_by_blocks;
  for (const auto& new_vote : new_pbft_rewards_votes) {
    new_rewards_votes_by_blocks[new_vote->getBlockHash()].push_back(new_vote);
  }

  // TODO: this could be probably optimised so actual db period is not read from db based on block hash:
  //       1. Either save few last pbft blocks periods
  //       2. or save somehow the period in RewardsVotes internal structures
  // Adds new votes into the period_data & cert_votes_period db columns
  for (const auto& new_rewards_votes_period : new_rewards_votes_by_blocks) {
    uint64_t period;
    if (auto pbft_block = db_->getPbftBlock(new_rewards_votes_period.first); pbft_block.has_value()) {
      period = pbft_block->getPeriod();
    } else {
      // This should never happen -> pbft block contains votes for blocks that do not exist in db
      assert(false);
      throw std::runtime_error("Pbft block contains rewards votes for nonexistent block");
    }

    // Gets period data
    auto period_raw_data = db_->getPeriodDataRaw(period);

    // Parses period data
    SyncBlock syncBlock(period_raw_data);

    // TODO: is there some more efficient way to do this ? There is merge operator in rocksdb but still votes need to
    //       be loaded in order to properly setup position of new votes inside period data
    // Appends new cert votes into the period data object
    uint32_t new_vote_pos = syncBlock.getCertVotes().size();
    for (const auto& new_vote : new_rewards_votes_period.second) {
      assert(new_vote->getBlockHash() == syncBlock.getPbftBlock()->getBlockHash());

      db_->addCertVotePeriodToBatch(new_vote->getHash(), period, new_vote_pos, batch);
      syncBlock.addCertVote(new_vote);
      new_vote_pos++;
    }

    // Re-save adjusted period data into the db
    db_->savePeriodData(syncBlock, batch);
  }

  // Commits DB write batch
  db_->commitWriteBatch(batch);

  // Appends new_pbft_rewards_votes into the blocks_cert_votes_ container
  appendNewVotesToCertVotes(new_rewards_votes_by_blocks);

  // Removes new_pbft_rewards_votes from new_processed_votes_
  removeNewProcessedVotes(new_pbft_rewards_votes);
}

std::vector<vote_hash_t> RewardsVotes::popUnrewardedVotes(size_t count) {
  {
    std::shared_lock lock(unrewarded_votes_mutex_);
    if (unrewarded_votes_.empty()) {
      return {};
    }
  }

  std::vector<vote_hash_t> selected_votes;

  {
    std::scoped_lock lock(unrewarded_votes_mutex_);

    // Can pop count votes only if there is enough votes inside unrewarded_votes_
    size_t votes_to_pop_count = std::min(count, unrewarded_votes_.size());
    selected_votes.reserve(votes_to_pop_count);

    auto it = unrewarded_votes_.begin();
    for (size_t idx = 0; idx < votes_to_pop_count; idx++) {
      assert(it != unrewarded_votes_.end());

      selected_votes.push_back(std::move(*it));
      it = unrewarded_votes_.erase(it);
    }
  }

  return selected_votes;
}

void RewardsVotes::removeUnrewardedVotes(const std::vector<vote_hash_t>& votes) {
  if (votes.empty()) {
    return;
  }

  std::scoped_lock lock(unrewarded_votes_mutex_);
  if (unrewarded_votes_.empty()) {
    return;
  }

  for (const auto& vote_hash : votes) {
    unrewarded_votes_.erase(vote_hash);
  }
}

std::pair<bool, std::string> RewardsVotes::checkBlockRewardsVotes(const std::vector<vote_hash_t>& block_rewards_votes) {
  auto unknown_votes_hashes = filterUnknownVotesHashes(block_rewards_votes);
  if (unknown_votes_hashes.empty()) {
    return {true, ""};
  }

  std::string err_msg = "Missing votes: ";
  for (const auto& vote_hash : unknown_votes_hashes) {
    err_msg += vote_hash.abridged() + ", ";
  }

  return {false, std::move(err_msg)};
}

std::vector<std::shared_ptr<Vote>> RewardsVotes::filterUnknownVotes(
    const std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>& votes) {
  std::vector<vote_hash_t> hashes;
  hashes.reserve(votes.size());
  for (const auto& vote_data : votes) {
    hashes.push_back(vote_data.first);
  }

  // All votes are known
  auto unknown_votes_hashes = filterUnknownVotesHashes(hashes);
  if (unknown_votes_hashes.empty()) {
    return {};
  }

  // Returns unknown votes
  std::vector<std::shared_ptr<Vote>> unknown_votes;
  unknown_votes.reserve(unknown_votes_hashes.size());
  for (const auto& uknown_vote_hash : unknown_votes_hashes) {
    unknown_votes.push_back(votes.at(uknown_vote_hash));
  }

  return unknown_votes;
}

std::vector<std::shared_ptr<Vote>> RewardsVotes::filterNewVotes(
    const std::vector<vote_hash_t>& dag_block_rewards_votes) {
  std::vector<std::shared_ptr<Vote>> filtered_votes;

  if (dag_block_rewards_votes.empty()) {
    return filtered_votes;
  }

  std::shared_lock lock(new_votes_mutex_);
  if (new_votes_.empty()) {
    return filtered_votes;
  }

  for (const auto& vote_hash : dag_block_rewards_votes) {
    if (auto found_vote = new_votes_.find(vote_hash); found_vote != new_votes_.end()) {
      filtered_votes.push_back(found_vote->second);
    }
  }

  return filtered_votes;
}

void RewardsVotes::markNewVotesAsProcessed(const std::vector<std::shared_ptr<Vote>>& dag_block_new_rewards_votes) {
  // All votes from dag_block_new_rewards_votes must be in new_votes_
  {
    std::shared_lock lock(new_votes_mutex_);
    for (const auto& vote : dag_block_new_rewards_votes) {
      assert(new_votes_.contains(vote->getHash()));
    }
  }

  // Adds dag_block_new_rewards_votes into the new_processed_votes_
  {
    std::scoped_lock lock(new_processed_votes_mutex_);
    for (const auto& vote : dag_block_new_rewards_votes) {
      new_processed_votes_.emplace(vote->getHash(), vote);
    }
  }

  // Removes dag_block_new_rewards_votes from the new_votes_
  {
    std::scoped_lock lock(new_votes_mutex_);
    for (const auto& vote : dag_block_new_rewards_votes) {
      new_votes_.erase(vote->getHash());
    }
  }
}

std::pair<bool, std::unordered_map<vote_hash_t, std::shared_ptr<Vote>>> RewardsVotes::getVotes(
    std::unordered_set<vote_hash_t>&& votes_hashes) {
  std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> result_votes;

  if (votes_hashes.empty()) {
    return std::make_pair(true, std::move(result_votes));
  }

  // Searches cached votes from provided container and insert found ones into the result_votes. In case all votes were
  // found, returns true, otherwise false
  auto searchForVotes = [&result_votes, &votes_hashes](
                            const std::unordered_map<blk_hash_t, std::shared_ptr<Vote>>& cached_votes) -> bool {
    for (auto it = votes_hashes.begin(); it != votes_hashes.end();) {
      auto found_vote = cached_votes.find(*it);
      if (found_vote == cached_votes.end()) {
        it++;
        continue;
      }

      std::cout << "found vote: " << it->abridged() << std::endl;
      result_votes.emplace(*found_vote);
      it = votes_hashes.erase(it);
    }

    return votes_hashes.empty();
  };

  // Returns missing votes
  auto getMissingVotes = [&votes_hashes]() -> std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> {
    std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> missing_votes;
    missing_votes.reserve(votes_hashes.size());
    for (const auto& missing_vote_hash : votes_hashes) {
      missing_votes.emplace(missing_vote_hash, nullptr);
    }

    return missing_votes;
  };

  // Checks cached cert votes
  {
    std::shared_lock lock(blocks_cert_votes_mutex_);
    for (const auto& block_cert_votes : blocks_cert_votes_) {
      if (searchForVotes(block_cert_votes.second)) {
        return std::make_pair(true, std::move(result_votes));
      }
    }
  }

  // Checks new_votes_
  {
    std::shared_lock lock(new_votes_mutex_);
    if (searchForVotes(new_votes_)) {
      return std::make_pair(true, std::move(result_votes));
    }
  }

  // Checks new_processed_votes_
  {
    std::shared_lock lock(new_processed_votes_mutex_);
    if (searchForVotes(new_processed_votes_)) {
      return std::make_pair(true, std::move(result_votes));
    }
  }

  // Checks db
  auto db_votes_periods = db_->getCertVotesPeriods(votes_hashes);
  // Not all votes periods were found
  if (db_votes_periods.size() != votes_hashes.size()) {
    // Filters out all that were found
    for (const auto& db_vote_data : db_votes_periods) {
      votes_hashes.erase(db_vote_data.first);
    }

    return std::make_pair(false, getMissingVotes());
  }

  auto db_votes = db_->getCertVotes(db_votes_periods);
  // Not all votes in period_data were found
  if (db_votes.size() != db_votes_periods.size()) {
    // Filters out all that were found
    for (const auto& db_vote : db_votes) {
      votes_hashes.erase(db_vote.first);
    }

    return std::make_pair(false, getMissingVotes());
  }

  // All votes were found
  for (auto& db_vote : db_votes) {
    result_votes.emplace(std::move(db_vote.first), std::move(db_vote.second));
    // No need to erase from votes_hashes as there is no more processing after this
  }

  return std::make_pair(true, std::move(result_votes));
}

std::unordered_set<vote_hash_t> RewardsVotes::filterUnknownVotesHashes(const std::vector<vote_hash_t>& votes_hashes) {
  std::unordered_set<vote_hash_t> unknown_votes;
  for (const auto& vote_hash : votes_hashes) {
    unknown_votes.insert(vote_hash);
  }

  // Checks votes container. In case all vote have been found in container, true is returned, otherwise false
  auto checkVotesContainer = [&unknown_votes](const auto& container) -> bool {
    for (auto it = unknown_votes.begin(); it != unknown_votes.end();) {
      if (container.contains(*it)) {
        it = unknown_votes.erase(it);
      } else {
        it++;
      }
    }

    return unknown_votes.empty();
  };

  // Checks blocks_cert_votes_
  {
    std::shared_lock lock(blocks_cert_votes_mutex_);
    for (const auto& period_cert_votes : blocks_cert_votes_) {
      if (checkVotesContainer(period_cert_votes.second)) {
        return unknown_votes;
      }
    }
  }

  // Checks new_votes_
  {
    std::shared_lock lock(new_votes_mutex_);
    if (checkVotesContainer(new_votes_)) {
      return unknown_votes;
    }
  }

  // Checks new_processed_votes_
  {
    std::shared_lock lock(new_processed_votes_mutex_);
    if (checkVotesContainer(new_processed_votes_)) {
      return unknown_votes;
    }
  }

  // Check db
  unknown_votes = db_->certVotesInDb(unknown_votes);
  if (unknown_votes.empty()) {
    return unknown_votes;
  }

  return unknown_votes;
}

std::vector<std::shared_ptr<Vote>> RewardsVotes::filterNewProcessedVotes(
    const std::unordered_set<vote_hash_t>& dag_blocks_rewards_votes) {
  std::vector<std::shared_ptr<Vote>> filtered_votes;

  if (dag_blocks_rewards_votes.empty()) {
    return filtered_votes;
  }

  std::shared_lock lock(new_processed_votes_mutex_);
  if (new_processed_votes_.empty()) {
    return filtered_votes;
  }

  for (const auto& vote_hash : dag_blocks_rewards_votes) {
    if (auto found_vote = new_processed_votes_.find(vote_hash); found_vote != new_processed_votes_.end()) {
      filtered_votes.push_back(found_vote->second);
    }
  }

  return filtered_votes;
}

void RewardsVotes::removeNewProcessedVotes(const std::vector<std::shared_ptr<Vote>>& votes_to_be_removed) {
  if (votes_to_be_removed.empty()) {
    return;
  }

  std::scoped_lock lock(new_processed_votes_mutex_);
  if (new_processed_votes_.empty()) {
    return;
  }

  for (const auto& vote : votes_to_be_removed) {
    new_processed_votes_.erase(vote->getHash());
  }
}

void RewardsVotes::appendNewVotesToCertVotes(
    const std::unordered_map<blk_hash_t, std::vector<std::shared_ptr<Vote>>>& blocks_votes) {
  if (blocks_votes.empty()) {
    return;
  }

  std::scoped_lock lock(blocks_cert_votes_mutex_);
  for (const auto& block_votes : blocks_votes) {
    auto cached_block_votes = blocks_cert_votes_.find(block_votes.first);
    if (cached_block_votes == blocks_cert_votes_.end()) {
      continue;
    }

    for (const auto& vote : block_votes.second) {
      cached_block_votes->second.emplace(vote->getHash(), vote);
    }
  }
}

bool RewardsVotes::isKnownVote(const std::shared_ptr<Vote>& vote) const {
  const auto voted_block_hash = vote->getBlockHash();
  const auto vote_hash = vote->getHash();

  // Checks blocks_cert_votes_
  {
    std::shared_lock lock(blocks_cert_votes_mutex_);
    if (auto cached_block_votes = blocks_cert_votes_.find(voted_block_hash);
        cached_block_votes != blocks_cert_votes_.end()) {
      if (cached_block_votes->second.contains(vote_hash)) {
        return true;
      }
    }
  }

  // Checks new_votes_
  {
    std::shared_lock lock(new_votes_mutex_);
    if (new_votes_.contains(vote_hash)) {
      return true;
    }
  }

  // Checks new_processed_votes_
  {
    std::shared_lock lock(new_processed_votes_mutex_);
    if (new_processed_votes_.contains(vote_hash)) {
      return true;
    }
  }

  // Checks db
  if (db_->isCertVoteInDb(vote_hash)) {
    return true;
  }

  return false;
}

void RewardsVotes::insertVotes(std::vector<std::shared_ptr<Vote>>&& votes) {
  std::scoped_lock lock(new_votes_mutex_);
  for (auto& vote : votes) {
    auto vote_hash = vote->getHash();
    new_votes_.emplace(std::move(vote_hash), std::move(vote));
  }
}

}  // namespace taraxa