#include "final_chain/rewards_stats.hpp"

namespace taraxa {

RewardsStats::RewardsStats(uint32_t expected_max_tx_count, uint32_t expected_max_validators_count) {
  if (expected_max_tx_count > 0) {
    txs_validators_.reserve(expected_max_tx_count);
  }

  if (expected_max_validators_count > 0) {
    txs_stats_.reserve(expected_max_validators_count);
    votes_stats_.reserve(expected_max_validators_count);
    unique_votes_.reserve(expected_max_validators_count);
  }
}

bool RewardsStats::addTransaction(const trx_hash_t& tx_hash, const addr_t& validator) {
  auto found_tx = txs_validators_.find(tx_hash);

  // Already processed tx
  if (found_tx != txs_validators_.end()) {
    return false;
  }

  // New tx
  txs_validators_[tx_hash] = validator;
  total_unique_txs_count_++;

  // Increment validator's unique txs count
  txs_stats_[validator]++;

  return true;
}

void RewardsStats::removeTransaction(const trx_hash_t& tx_hash) {
  auto found_tx = txs_validators_.find(tx_hash);
  assert(found_tx != txs_validators_.end());

  auto found_validator = txs_stats_.find(found_tx->second);
  assert(found_validator != txs_stats_.end());

  assert(found_validator->second);

  found_validator->second--;
  total_unique_txs_count_--;
}

std::optional<addr_t> RewardsStats::getTransactionValidator(const trx_hash_t& tx_hash) {
  auto found_tx = txs_validators_.find(tx_hash);
  if (found_tx == txs_validators_.end()) {
    return {};
  }

  return {found_tx->second};
}

bool RewardsStats::addVote(const Vote& vote) {
  auto found_vote = unique_votes_.find(vote.getHash());

  // Already processed vote
  if (found_vote != unique_votes_.end()) {
    return false;
  }

  // New vote
  unique_votes_.insert(vote.getHash());
  total_unique_votes_count_++;

  // Increment validator's unique vote count
  votes_stats_[vote.getVoterAddr()]++;
  return true;
}

RLP_FIELDS_DEFINE(RewardsStats, txs_stats_, total_unique_txs_count_, votes_stats_, total_unique_votes_count_)

}  // namespace taraxa
