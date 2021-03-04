#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "data.hpp"
#include "logger/log.hpp"
#include "storage/db.hpp"

/**
 * In pbft_chain, two kinds of blocks:
 * 1. PivotBlock: determine DAG block in pivot chain
 * 2. ScheduleBlock: determine sequential/concurrent set
 */
namespace taraxa {

class PbftChain {
 public:
  explicit PbftChain(std::string const& dag_genesis_hash, addr_t node_addr, std::shared_ptr<DB> db);

  blk_hash_t getHeadHash() const;
  uint64_t getPbftChainSize() const;
  blk_hash_t getLastPbftBlockHash() const;

  PbftBlock getPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  std::shared_ptr<PbftBlock> getUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash);
  std::vector<PbftBlockCert> getPbftBlocks(size_t period, size_t count);
  std::vector<std::string> getPbftBlocksStr(size_t period, size_t count, bool hash) const;  // Remove
  std::string getJsonStr() const;

  bool findPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  bool findUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash) const;
  bool findPbftBlockInSyncedSet(blk_hash_t const& pbft_block_hash) const;

  void cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block);
  void pushUnverifiedPbftBlock(std::shared_ptr<PbftBlock> const& pbft_block);

  void updatePbftChain(blk_hash_t const& pbft_block_hash);

  bool checkPbftBlockValidationFromSyncing(taraxa::PbftBlock const& pbft_block) const;
  bool checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const;

  // Syncing
  uint64_t pbftSyncingPeriod() const;
  bool pbftSyncedQueueEmpty() const;
  PbftBlockCert pbftSyncedQueueFront() const;
  PbftBlockCert pbftSyncedQueueBack() const;
  void pbftSyncedQueuePopFront();
  void setSyncedPbftBlockIntoQueue(PbftBlockCert const& pbft_block_and_votes);
  void clearSyncedPbftBlocks();
  size_t pbftSyncedQueueSize() const;
  bool isKnownPbftBlockForSyncing(blk_hash_t const& pbft_block_hash);
  std::shared_ptr<PbftBlock> getPbftBlock(uint64_t pbft_period);

 private:
  void pbftSyncedSetInsert_(blk_hash_t const& pbft_block_hash);
  void pbftSyncedSetErase_();
  void insertUnverifiedPbftBlockIntoParentMap_(blk_hash_t const& prev_block_hash, blk_hash_t const& block_hash);

  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  mutable boost::shared_mutex sync_access_;
  mutable boost::shared_mutex unverified_access_;
  mutable boost::shared_mutex chain_head_access_;

  blk_hash_t head_hash_;             // PBFT head hash
  blk_hash_t dag_genesis_hash_;      // DAG genesis at height 1
  uint64_t size_;                    // PBFT chain size, includes both executed and unexecuted PBFT blocks
  blk_hash_t last_pbft_block_hash_;  // last PBFT block hash in PBFT chain, may not execute yet

  std::shared_ptr<DB> db_ = nullptr;

  // <prev block hash, vector<PBFT proposed blocks waiting for vote>>
  std::unordered_map<blk_hash_t, std::vector<blk_hash_t>> unverified_blocks_map_;
  std::unordered_map<blk_hash_t, std::shared_ptr<PbftBlock>> unverified_blocks_;

  // syncing pbft blocks from peers
  std::deque<PbftBlockCert> pbft_synced_queue_;
  std::unordered_set<blk_hash_t> pbft_synced_set_;

  LOG_OBJECTS_DEFINE;
};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

}  // namespace taraxa
