#include "final_chain/dag_stats.hpp"

namespace taraxa {

DagStats::DagStats(uint32_t expected_max_trx_count) {
  if (expected_max_trx_count > 0) {
    transactions_stats_map_.reserve(expected_max_trx_count);
  }
}

void DagStats::addDagBlock(const addr_t& block_author) {
  blocks_stats_.proposers_blocks_count_[block_author]++;
  blocks_stats_.total_blocks_count_++;
}

const DagStats::BlocksStats& DagStats::getBlocksStats() const { return blocks_stats_; }

bool DagStats::hasTransactionStats(const trx_hash_t& tx_hash) const {
  return transactions_stats_map_.find(tx_hash) != transactions_stats_map_.end();
}

const DagStats::TransactionStats& DagStats::getTransactionStats(const trx_hash_t& tx_hash) const {
  const auto found_tx = transactions_stats_map_.find(tx_hash);
  if (found_tx == transactions_stats_map_.end()) {
    throw std::runtime_error("DagStats::getTransactionStats trying to access non-existent transaction stats.");
  }

  return found_tx->second;
}

DagStats::TransactionStats&& DagStats::getTransactionStatsRvalue(const trx_hash_t& tx_hash) {
  const auto found_tx = transactions_stats_map_.find(tx_hash);
  if (found_tx == transactions_stats_map_.end()) {
    throw std::runtime_error("DagStats::getTransactionStats trying to access non-existent transaction stats.");
  }

  return std::move(found_tx->second);
}

bool DagStats::addTransaction(const trx_hash_t& tx_hash, const addr_t& inclusion_block_author) {
  auto found_tx = transactions_stats_map_.find(tx_hash);

  // New tx
  if (found_tx == transactions_stats_map_.end()) {
    transactions_stats_map_[tx_hash] = {inclusion_block_author, {}};
    return false;
  }

  // Already processed tx
  found_tx->second.uncle_proposers_.emplace_back(inclusion_block_author);
  return true;
}

void DagStats::clear() {
  transactions_stats_map_.clear();
  blocks_stats_.total_blocks_count_ = 0;
  blocks_stats_.proposers_blocks_count_.clear();
}

RLP_FIELDS_DEFINE(DagStats::BlocksStats, proposers_blocks_count_, total_blocks_count_)
RLP_FIELDS_DEFINE(DagStats::TransactionStats, proposer_, uncle_proposers_)

}  // namespace taraxa
