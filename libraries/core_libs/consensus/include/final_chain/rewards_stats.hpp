#pragma once

#include <map>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "vote/vote.hpp"

namespace taraxa {

/**
 * @class RewardsStats
 * @brief RewardsStats contains rewards statistics for single pbft block
 */
class RewardsStats {
 public:
  RewardsStats() = default;
  RewardsStats(uint32_t expected_max_tx_count, uint32_t expected_max_validators_count);
  ~RewardsStats() = default;

  /**
   * @brief In case unique tx_hash is provided, it is mapped to it's validator's address + validator's unique txs count
   *        is incremented. If provided tx_hash was already processed, nothing happens
   *
   * @param tx_hash
   * @param validator
   * @return true in case tx_hash was unique and processed, otherwise false
   */
  bool addTransaction(const trx_hash_t& tx_hash, const addr_t& validator);

  /**
   * @brief Remove tx from statistics
   *
   * @param tx_hash
   */
  void removeTransaction(const trx_hash_t& tx_hash);

  /**
   * @brief In case unique vote is provided, it's author's unique votes count is incremented. If provided vote was
   *        already processed, nothing happens
   *
   * @param vote
   * @return true in case vote was unique and processed, otherwise false
   */
  bool addVote(const Vote& vote);

 public:
  HAS_RLP_FIELDS

 private:
  // Transactions stats: tx hash -> validator that included it as first in his block
  std::unordered_map<trx_hash_t, addr_t> transactions_stats_;

  // Blocks stats: validator -> how many unique txs he included in his blocks
  // Unique txs is what defines quality of block -> block with 10 unique transactions is 10 times more valuable
  // than block with single unique transaction
  std::unordered_map<addr_t, uint32_t> blocks_stats_;

  // Votes stats: validator -> how many valid votes he sent during consensus process
  std::unordered_map<addr_t, uint32_t> votes_stats_;

  // Unique votes counter
  uint32_t unique_votes_count_{0};

  // Cache for unique votes - not sent in rlp !
  std::unordered_set<vote_hash_t> unique_votes_;
};

}  // namespace taraxa
