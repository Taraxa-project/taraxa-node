#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include "common/types.hpp"
#include "dag/dag_block.hpp"
#include "vote/vote.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

/**
 * @brief The PbftBlock class is a PBFT block class that includes PBFT block hash, previous PBFT block hash, DAG anchor
 * hash, DAG blocks ordering hash, period number, timestamp, proposer address, and proposer signature.
 */
class PbftBlock {
  blk_hash_t block_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_as_pivot_;
  blk_hash_t order_hash_;
  blk_hash_t prev_state_root_hash_;
  PbftPeriod period_;  // Block index, PBFT head block is period 0, first PBFT block is period 1
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t signature_;
  std::vector<vote_hash_t> reward_votes_;  // Cert votes in previous period

 public:
  PbftBlock(const blk_hash_t& prev_blk_hash, const blk_hash_t& dag_blk_hash_as_pivot, const blk_hash_t& order_hash,
            const blk_hash_t& prev_state_root, PbftPeriod period, const addr_t& beneficiary, const secret_t& sk,
            std::vector<vote_hash_t>&& reward_votes);
  explicit PbftBlock(const dev::RLP& rlp);
  explicit PbftBlock(const bytes& RLP);

  /**
   * @brief Secure Hash Algorithm 3
   * @param include_sig if include signature
   * @return secure hash as PBFT block hash
   */
  blk_hash_t sha3(bool include_sig) const;

  /**
   * @brief Get PBFT block in string
   * @return PBFT block in string
   */
  std::string getJsonStr() const;

  /**
   * @brief Get PBFT block in JSON
   * @return PBFT block in JSON
   */
  Json::Value getJson() const;

  /**
   * @brief Stream RLP uses to setup PBFT block hash
   * @param strm RLP stream
   * @param include_sig if include signature
   */
  void streamRLP(dev::RLPStream& strm, bool include_sig) const;

  /**
   * @brief Recursive Length Prefix
   * @param include_sig if include signature
   * @return bytes of RLP stream
   */
  bytes rlp(bool include_sig) const;

  /**
   * @brief Get PBFT block with DAG blocks in JSON
   * @param b PBFT block
   * @param dag_blks DAG blocks hashes
   * @return PBFT block with DAG blocks in JSON
   */
  static Json::Value toJson(const PbftBlock& b, const std::vector<blk_hash_t>& dag_blks);

  /**
   * @brief Get PBFT block hash
   * @return PBFT block hash
   */
  const auto& getBlockHash() const { return block_hash_; }

  /**
   * @brief Get previous PBFT block hash
   * @return previous PBFT block hash
   */
  const auto& getPrevBlockHash() const { return prev_block_hash_; }

  /**
   * @brief Get DAG anchor hash for the finalized PBFT block
   * @return DAG anchor hash
   */
  const auto& getPivotDagBlockHash() const { return dag_block_hash_as_pivot_; }

  /**
   * @brief Get DAG blocks ordering hash
   * @return DAG blocks ordering hash
   */
  const auto& getOrderHash() const { return order_hash_; }

  const auto& getPrevStateRoot() const { return prev_state_root_hash_; }

  /**
   * @brief Get period number
   * @return period number
   */
  PbftPeriod getPeriod() const { return period_; }

  /**
   * @brief Get timestamp
   * @return timestamp
   */
  auto getTimestamp() const { return timestamp_; }

  /**
   * @brief Get PBFT block proposer address
   * @return PBFT block proposer address
   */
  const auto& getBeneficiary() const { return beneficiary_; }

  const auto& getRewardVotes() const { return reward_votes_; }

 private:
  /**
   * @brief Set PBFT block hash and block proposer address
   */
  void calculateHash_();

  /**
   * @brief Check if all rewards votes are unique
   *
   */
  void checkUniqueRewardVotes();
};
std::ostream& operator<<(std::ostream& strm, const PbftBlock& pbft_blk);

/** @}*/

}  // namespace taraxa