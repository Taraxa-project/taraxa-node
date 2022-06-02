#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "config/pbft_config.hpp"
#include "logger/logger.hpp"
#include "pbft/pbft_block.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class DbStorage;
class FullNode;
class Vote;
class DagBlock;
struct Transaction;

/**
 * @brief PbftChain class is a single linked list that contains finalized PBFT blocks
 */
class PbftChain {
 public:
  explicit PbftChain(addr_t node_addr, std::shared_ptr<DbStorage> db);

  /**
   * @brief Get PBFT chain head hash
   * @return PBFT chain head hash
   */
  blk_hash_t getHeadHash() const;

  /**
   * @brief Get PBFT chain size
   * @return PBFT chain size
   */
  uint64_t getPbftChainSize() const;

  /**
   * @brief Get PBFT chain size excluding empty PBFT blocks
   * @return PBFT chain size excluding empty PBFT blocks
   */
  uint64_t getPbftChainSizeExcludingEmptyPbftBlocks() const;

  /**
   * @brief Get last PBFT block hash
   * @return last PBFT block hash
   */
  blk_hash_t getLastPbftBlockHash() const;

  /**
   * @brief Get a PBFT block in chain
   * @param pbft_block_hash PBFT block hash
   * @return PBFT block
   */
  PbftBlock getPbftBlockInChain(blk_hash_t const& pbft_block_hash);

  /**
   * @brief Get a unverified PBFT block
   * @param pbft_block_hash PBFT block hash
   * @return a unverified PBFT block
   */
  std::shared_ptr<PbftBlock> getUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash);

  /**
   * @brief Get PBFT chain head block in JSON string
   * @return PBFT chain head block in string
   */
  std::string getJsonStr() const;

  /**
   * @brief Get PBFT chain head block in JSON string
   * @param block_hash last PBFT block hash
   * @param null_anchor if the PBFT block include an empty DAG anchor
   * @return PBFT chain head block in string
   */
  std::string getJsonStrForBlock(blk_hash_t const& block_hash, bool null_anchor) const;

  /**
   * @brief Find a PBFT block in chain
   * @param pbft_block_hash PBFT block hash
   * @return true if found
   */
  bool findPbftBlockInChain(blk_hash_t const& pbft_block_hash);

  /**
   * @brief Find a unverified PBFT block
   * @param pbft_block_hash PBFT block hash
   * @return true if found
   */
  bool findUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash) const;

  /**
   * @brief Cleanup all proposed PBFT blocks for the finalized period
   * @param pbft_block last finalized PBFT block
   */
  void cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block);

  /**
   * @brief Push unfinalized PBFT block in PBFT unverified queue
   * @param pbft_block an unfinalized PBFT block
   * @return true if pushed
   */
  bool pushUnverifiedPbftBlock(std::shared_ptr<PbftBlock> const& pbft_block);

  /**
   * @brief Update PBFT chain size, non empty chain size, and last PBFT block hash
   * @param pbft_block_hash last PBFT block hash
   * @param null_anchor if the PBFT block include an empty DAG anchor
   */
  void updatePbftChain(blk_hash_t const& pbft_block_hash, bool null_anchor = false);

  /**
   * @brief Verify a PBFT block
   * @param pbft_block PBFT block
   * @return true if passed verification
   */
  bool checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const;

 private:
  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;
  using UpgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using UpgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  mutable boost::shared_mutex unverified_access_;
  mutable boost::shared_mutex chain_head_access_;

  blk_hash_t head_hash_;     // PBFT head hash
  uint64_t size_;            // PBFT chain size, includes both executed and unexecuted PBFT blocks
  uint64_t non_empty_size_;  // PBFT chain size excluding blocks with null anchor, includes both executed and unexecuted
                             // PBFT blocks
  blk_hash_t last_pbft_block_hash_;  // last PBFT block hash in PBFT chain, may not execute yet

  std::shared_ptr<DbStorage> db_ = nullptr;

  // <prev block hash, vector<PBFT proposed blocks waiting for vote>>
  std::unordered_map<blk_hash_t, std::vector<blk_hash_t>> unverified_blocks_map_;
  std::unordered_map<blk_hash_t, std::shared_ptr<PbftBlock>> unverified_blocks_;

  LOG_OBJECTS_DEFINE
};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

/** @}*/

}  // namespace taraxa
