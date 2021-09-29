#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "config/pbft_config.hpp"
#include "logger/logger.hpp"
#include "pbft/pbft_block.hpp"

/**
 * In pbft_chain, two kinds of blocks:
 * 1. PivotBlock: determine DAG block in pivot chain
 * 2. ScheduleBlock: determine sequential/concurrent set
 */
namespace taraxa {

class DbStorage;
class FullNode;
class Vote;
class DagBlock;
struct Transaction;

class PbftChain {
 public:
  explicit PbftChain(blk_hash_t const& dag_genesis_hash, addr_t node_addr, std::shared_ptr<DbStorage> db);

  blk_hash_t getHeadHash() const;
  uint64_t getPbftChainSize() const;
  blk_hash_t getLastPbftBlockHash() const;

  PbftBlock getPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  std::shared_ptr<PbftBlock> getUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash);
  std::vector<std::string> getPbftBlocksStr(size_t period, size_t count, bool hash) const;  // Remove
  std::string getJsonStr() const;
  std::string getJsonStrForBlock(blk_hash_t const& block_hash) const;

  bool findPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  bool findUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash) const;
  bool findPbftBlockInSyncedSet(blk_hash_t const& pbft_block_hash) const;

  void cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block);
  bool pushUnverifiedPbftBlock(std::shared_ptr<PbftBlock> const& pbft_block);

  void updatePbftChain(blk_hash_t const& pbft_block_hash);

  bool checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const;

 private:
  void insertUnverifiedPbftBlockIntoParentMap_(blk_hash_t const& prev_block_hash, blk_hash_t const& block_hash);

  using UniqueLock = boost::unique_lock<boost::shared_mutex>;
  using SharedLock = boost::shared_lock<boost::shared_mutex>;
  using UpgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using UpgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  mutable boost::shared_mutex unverified_access_;
  mutable boost::shared_mutex chain_head_access_;

  blk_hash_t head_hash_;             // PBFT head hash
  blk_hash_t dag_genesis_hash_;      // DAG genesis at height 1
  uint64_t size_;                    // PBFT chain size, includes both executed and unexecuted PBFT blocks
  blk_hash_t last_pbft_block_hash_;  // last PBFT block hash in PBFT chain, may not execute yet

  std::shared_ptr<DbStorage> db_ = nullptr;

  // <prev block hash, vector<PBFT proposed blocks waiting for vote>>
  std::unordered_map<blk_hash_t, std::vector<blk_hash_t>> unverified_blocks_map_;
  std::unordered_map<blk_hash_t, std::shared_ptr<PbftBlock>> unverified_blocks_;

  LOG_OBJECTS_DEFINE
};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

}  // namespace taraxa
