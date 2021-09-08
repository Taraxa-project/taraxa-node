#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <boost/thread/shared_mutex.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "common/types.hpp"
#include "logger/log.hpp"
#include "pbft_config.hpp"

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

enum PbftVoteTypes : uint8_t { propose_vote_type = 0, soft_vote_type, cert_vote_type, next_vote_type };

class PbftBlock {
  blk_hash_t block_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_as_pivot_;
  uint64_t period_;  // Block index, PBFT head block is period 0, first PBFT block is period 1
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t signature_;

 public:
  PbftBlock(blk_hash_t const& prev_blk_hash, blk_hash_t const& dag_blk_hash_as_pivot, uint64_t period,
            addr_t const& beneficiary, secret_t const& sk);
  explicit PbftBlock(dev::RLP const& r);
  explicit PbftBlock(bytes const& RLP);
  explicit PbftBlock(std::string const& JSON);

  blk_hash_t sha3(bool include_sig) const;
  std::string getJsonStr() const;
  Json::Value getJson() const;
  void streamRLP(dev::RLPStream& strm, bool include_sig) const;
  bytes rlp(bool include_sig) const;

  static Json::Value toJson(PbftBlock const& b, std::vector<blk_hash_t> const& dag_blks);

  auto const& getBlockHash() const { return block_hash_; }
  auto const& getPrevBlockHash() const { return prev_block_hash_; }
  auto const& getPivotDagBlockHash() const { return dag_block_hash_as_pivot_; }
  auto getPeriod() const { return period_; }
  auto getTimestamp() const { return timestamp_; }
  auto const& getBeneficiary() const { return beneficiary_; }

 private:
  void calculateHash_();
};
std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk);

struct PbftBlockCert {
  PbftBlockCert(PbftBlock const& pbft_blk, std::vector<Vote> const& cert_votes);
  explicit PbftBlockCert(dev::RLP const& all_rlp);
  explicit PbftBlockCert(bytes const& all_rlp);

  std::shared_ptr<PbftBlock> pbft_blk;
  std::vector<Vote> cert_votes;
  std::map<uint64_t, std::vector<DagBlock>> dag_blocks_per_level;
  std::vector<Transaction> transactions;
  bytes rlp() const;
};
std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b);

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

 private:
  void pbftSyncedSetInsert_(blk_hash_t const& pbft_block_hash);
  void pbftSyncedSetErase_();

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

  std::shared_ptr<DbStorage> db_ = nullptr;

  // <prev block hash, vector<PBFT proposed blocks waiting for vote>>
  std::unordered_map<blk_hash_t, std::vector<blk_hash_t>> unverified_blocks_map_;
  std::unordered_map<blk_hash_t, std::shared_ptr<PbftBlock>> unverified_blocks_;

  // syncing pbft blocks from peers
  std::deque<PbftBlockCert> pbft_synced_queue_;
  std::unordered_set<blk_hash_t> pbft_synced_set_;

  LOG_OBJECTS_DEFINE
};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

}  // namespace taraxa
