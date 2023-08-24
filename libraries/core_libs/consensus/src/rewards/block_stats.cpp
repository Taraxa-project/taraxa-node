#include "rewards/block_stats.hpp"

#include <cstdint>

#include "pbft/pbft_block.hpp"

namespace taraxa::rewards {

BlockStats::BlockStats(const PeriodData& block, uint64_t dpos_vote_count, uint32_t committee_size)
    : block_author_(block.pbft_blk->getBeneficiary()),
      max_votes_weight_(std::min<uint64_t>(committee_size, dpos_vote_count)) {
  processStats(block);
}

bool BlockStats::addTransaction(const trx_hash_t& tx_hash, const addr_t& validator) {
  auto found_tx = validator_by_tx_hash_.find(tx_hash);

  // Already processed tx
  if (found_tx != validator_by_tx_hash_.end()) {
    return false;
  }

  // New tx
  validator_by_tx_hash_[tx_hash] = validator;

  return true;
}

std::optional<addr_t> BlockStats::getTransactionValidator(const trx_hash_t& tx_hash) {
  auto found_tx = validator_by_tx_hash_.find(tx_hash);
  if (found_tx == validator_by_tx_hash_.end()) {
    return {};
  }

  return {found_tx->second};
}

bool BlockStats::addVote(const std::shared_ptr<Vote>& vote) {
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

std::set<trx_hash_t> toTrxHashesSet(const SharedTransactions& transactions) {
  std::set<trx_hash_t> block_transactions_hashes_;
  std::transform(transactions.begin(), transactions.end(),
                 std::inserter(block_transactions_hashes_, block_transactions_hashes_.begin()),
                 [](const auto& trx) { return trx->getHash(); });

  return block_transactions_hashes_;
}

void BlockStats::processStats(const PeriodData& block) {
  validator_by_tx_hash_.reserve(block.transactions.size());
  validators_stats_.reserve(std::max(block.dag_blocks.size(), block.previous_block_cert_votes.size()));
  auto block_transactions_hashes_ = toTrxHashesSet(block.transactions);

  for (const auto& dag_block : block.dag_blocks) {
    const addr_t& dag_block_author = dag_block.getSender();
    bool has_unique_transactions = false;
    for (const auto& tx_hash : dag_block.getTrxs()) {
      // we should also check that we have transactions in pbft block(period data). Because in dag blocks could be
      // included transaction that was finalized in previous blocks
      if (!block_transactions_hashes_.contains(tx_hash)) {
        continue;
      }

      // returns is transactions was inserted to txs_validators_(its first including into blockchain)
      if (addTransaction(tx_hash, dag_block_author)) {
        has_unique_transactions = true;
      }
    }
    if (has_unique_transactions) {
      validators_stats_[dag_block_author].dag_blocks_count_ += 1;
      total_dag_blocks_count_ += 1;
    }
  }
  // total_unique_txs_count_ should be always equal to transactions count in block
  assert(validator_by_tx_hash_.size() == block.transactions.size());

  for (const auto& vote : block.previous_block_cert_votes) {
    addVote(vote);
  }

  txs_validators_.reserve(block.transactions.size());
  for (const auto& tx : block.transactions) {
    // Non-executed trxs
    auto tx_validator = getTransactionValidator(tx->getHash());
    assert(tx_validator.has_value());

    txs_validators_.push_back(*tx_validator);
  }
}

RLP_FIELDS_DEFINE(BlockStats::ValidatorStats, dag_blocks_count_, vote_weight_)
RLP_FIELDS_DEFINE(BlockStats, block_author_, validators_stats_, txs_validators_, total_dag_blocks_count_,
                  total_votes_weight_, max_votes_weight_)

}  // namespace taraxa::rewards