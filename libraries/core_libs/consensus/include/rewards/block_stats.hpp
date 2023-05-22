#pragma once

#include <map>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"
#include "pbft/period_data.hpp"
#include "vote/vote.hpp"

namespace taraxa::rewards {

/**
 * @class RewardsStats
 * @brief RewardsStats contains rewards statistics for single pbft block
 */
class BlockStats {
 public:
  // Needed for RLP
  BlockStats() = default;
  /**
   * @brief setting block_author_, max_votes_weight_ and calls processStats function
   *
   * @param dpos_vote_count - votes count for previous block
   * @param committee_size
   */
  BlockStats(const PeriodData& block, const std::vector<gas_t>& trxs_gas_used, uint64_t dpos_vote_count,
             uint32_t committee_size);

  HAS_RLP_FIELDS

 private:
  /**
   * @brief Process PeriodData and save stats in class for future serialization. returns
   *
   * @param block
   */
  void processStats(const PeriodData& block);

  /**
   * @brief Prepare fee_by_trx_hash_ map with trx fee by trx hash
   *
   * @param transactions collection with transactions included in the block
   * @param trxs_gas_used collection with gas used by transactions
   */
  void initFeeByTrxHash(const SharedTransactions& transactions, const std::vector<gas_t>& trxs_gas_used);

  /**
   * @brief In case unique trx_hash is provided, it is mapped to it's validator's address + validator's unique trxs
   * count is incremented. If provided trx_hash was already processed, nothing happens
   *
   * @param trx_hash
   * @param validator
   * @return true in case trx_hash was unique and processed, otherwise false
   */
  bool addTransaction(const trx_hash_t& trx_hash, const addr_t& validator);

  /**
   * @brief In case unique vote is provided, author's votes weight is updated. If provided vote was
   *        already processed, nothing happens
   *
   * @param vote
   * @return true in case vote was unique and processed, otherwise false
   */
  bool addVote(const std::shared_ptr<Vote>& vote);

 protected:
  struct ValidatorStats {
    // count of rewardable(with 1 or more unique transactions) DAG blocks produced by this validator
    uint32_t dag_blocks_count_ = 0;

    // Validator cert voted block weight
    uint64_t vote_weight_ = 0;

    u256 fees_rewards_ = 0;

    HAS_RLP_FIELDS
  };

  // Pbft block author
  addr_t block_author_;

  // Fee by trx : trx hash -> fee
  std::unordered_map<trx_hash_t, u256> fee_by_trx_hash_;

  // Validator stats: validator -> ValidatorStats
  std::unordered_map<addr_t, ValidatorStats> validators_stats_;

  // Total rewardable(with 1 or more unique transactions) DAG blocks count
  uint32_t total_dag_blocks_count_{0};

  // Total weight of votes in block
  uint64_t total_votes_weight_{0};

  // Max weight of votes in block
  uint64_t max_votes_weight_{0};
};

}  // namespace taraxa::rewards