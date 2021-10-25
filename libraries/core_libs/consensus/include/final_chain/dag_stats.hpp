#pragma once

#include <bitset>
#include <map>
#include <vector>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"

namespace taraxa {

/**
 * @class DagStats
 * @brief DagStats contains mining rewards statistics based on created dag blocks and included transactions
 */
class DagStats {
 public:
  /**
   * @struct BlocksStats
   * @typedef BlocksStats consists of:
   *            - dag blocks rewards statistics. It maps proposer address -> number of valid dag blocks that he created
   *            - total number of dag blocks
   */
  struct BlocksStats {
    std::unordered_map<addr_t, uint32_t> proposers_blocks_count_;
    uint32_t total_blocks_count_{0};

    HAS_RLP_FIELDS
  };

  /**
   * @struct TransactionStats
   * @brief TxStats consists of transaction rewards statistics:
   *        proposer_          - proposer, who included tx_ in his dag block as first (his block was ordered as first)
   *        uncle_proposers_   - vector of proposers, who included tx_ in their dag blocks after proposer_
   */
  struct TransactionStats {
    addr_t proposer_{0};
    std::vector<addr_t> uncle_proposers_;

    HAS_RLP_FIELDS
  };

 public:
  DagStats(uint32_t expected_max_trx_count = 0);

  /**
   * @brief Increment proposer's block counter in stats
   *
   * @param block_author address of proposer
   */
  void addDagBlock(const addr_t& block_author);

  /**
   * @brief Get blocks rewards statistics
   *
   * @return const std::vector<TxData>&
   */
  const BlocksStats& getBlocksStats() const;

  /**
   * @brief hasTransactionStats
   *
   * @param tx_hash
   * @return true if stats for tx_hash exists, otherwise false
   */
  bool hasTransactionStats(const trx_hash_t& tx_hash) const;

  /**
   * @brief Get transaction rewards statistics
   *
   * @note Should be used together with hasTransactionStats(...)
   * @return TransactionsStats&
   */
  const TransactionStats& getTransactionStats(const trx_hash_t& tx_hash) const;

  /**
   * @brief Get transaction rewards statistics
   *
   * @note Should be used together with hasTransactionStats(...)
   * @note It returns rvalue, thus modifying internal container, use only if you know what you are doing
   * @return TransactionsStats&&
   */
  TransactionStats&& getTransactionStatsRvalue(const trx_hash_t& tx_hash);

  /**
   * @brief Add transaction - it adds proposer's address, who included this tx in his block to the rewards stats
   *
   * @param tx_hash
   * @param inclusion_block_author
   * @return true in case new unique tx was added, otherwise false
   */
  bool addTransaction(const trx_hash_t& tx_hash, const addr_t& inclusion_block_author);

  /**
   * @brief Clear internal containers with data
   */
  void clear();

 private:
  // Blocks Stats contains how many dag blocks each proposer created + total number of all dag blocks
  BlocksStats blocks_stats_;

  // Transactions stats
  std::unordered_map<trx_hash_t, TransactionStats> transactions_stats_map_;
};

}  // namespace taraxa
