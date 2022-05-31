#include "final_chain/rewards_stats.hpp"

namespace taraxa {

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
  validators_stats_[validator].unique_txs_count_++;

  return true;
}

void RewardsStats::removeTransaction(const trx_hash_t& tx_hash) {
  auto found_tx = txs_validators_.find(tx_hash);
  assert(found_tx != txs_validators_.end());

  auto found_validator = validators_stats_.find(found_tx->second);
  assert(found_validator != validators_stats_.end());

  assert(found_validator->second.unique_txs_count_);

  found_validator->second.unique_txs_count_--;
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
  // Set valid cert vote to validator
  auto& validator_stats = validators_stats_[vote.getVoterAddr()];
  assert(validator_stats.valid_cert_vote_ == false);

  validator_stats.valid_cert_vote_ = true;
  total_unique_votes_count_++;

  return true;
}

void RewardsStats::processStats(const SyncBlock& sync_blk) {
  txs_validators_.reserve(sync_blk.transactions.size());

  // TODO: use std::max(sync_blk.dag_blocks.size(), sync_blk.rewards_votes.size())
  validators_stats_.reserve(sync_blk.dag_blocks.size());

  for (const auto& dag_block : sync_blk.dag_blocks) {
    const addr_t& dag_block_author = dag_block.getSender();
    for (const auto& tx_hash : dag_block.getTrxs()) {
      addTransaction(tx_hash, dag_block_author);
    }
  }

  // TODO: add votes to rewards_stats
}

RLP_FIELDS_DEFINE(RewardsStats::ValidatorStats, unique_txs_count_, valid_cert_vote_)
RLP_FIELDS_DEFINE(RewardsStats, validators_stats_, total_unique_txs_count_, total_unique_votes_count_)

}  // namespace taraxa