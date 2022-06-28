#include "final_chain/rewards_stats.hpp"

#include <cstdint>

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

bool RewardsStats::addVote(const std::shared_ptr<Vote>& vote) {
  // Set valid cert vote to validator
  auto& validator_stats = validators_stats_[vote->getVoterAddr()];
  assert(validator_stats.vote_weight_ == 0);
  assert(vote->getWeight());

  if (validator_stats.vote_weight_) {
    return false;
  }

  const auto weight = *vote->getWeight();
  total_votes_weight_ += weight;
  validator_stats.vote_weight_ = weight;
  return true;
}

void RewardsStats::initStats(const PeriodData& sync_blk, uint64_t dpos_vote_count, uint32_t committee_size) {
  txs_validators_.reserve(sync_blk.transactions.size());
  validators_stats_.reserve(std::max(sync_blk.dag_blocks.size(), sync_blk.previous_block_cert_votes.size()));

  for (const auto& dag_block : sync_blk.dag_blocks) {
    const addr_t& dag_block_author = dag_block.getSender();
    for (const auto& tx_hash : dag_block.getTrxs()) {
      addTransaction(tx_hash, dag_block_author);
    }
  }
  max_votes_weight_ = std::min<uint64_t>(committee_size, dpos_vote_count);
  for (const auto& vote : sync_blk.previous_block_cert_votes) {
    addVote(vote);
  }
}

std::vector<addr_t> RewardsStats::processStats(const PeriodData& block, uint64_t dpos_vote_count,
                                               uint32_t committee_size) {
  initStats(block, dpos_vote_count, committee_size);

  // Dag blocks validators that included transactions to be executed as first in their blocks
  std::vector<addr_t> txs_validators;
  txs_validators.reserve(block.transactions.size());

  for (const auto& tx : block.transactions) {
    // TODO: if enabled, it would break current implementation of RewardsStats
    // if (replay_protection_service_ && replay_protection_service_->is_nonce_stale(trx->getSender(),
    //   trx->getNonce())) {
    //   removeTransaction(tx.getHash());
    //   continue;
    // }

    // Non-executed trxs
    auto tx_validator = getTransactionValidator(tx->getHash());
    assert(tx_validator.has_value());

    txs_validators.push_back(tx_validator.value());
  }

  return txs_validators;
}

RLP_FIELDS_DEFINE(RewardsStats::ValidatorStats, unique_txs_count_, vote_weight_)
RLP_FIELDS_DEFINE(RewardsStats, validators_stats_, total_unique_txs_count_, total_votes_weight_, max_votes_weight_)

}  // namespace taraxa