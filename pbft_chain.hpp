#ifndef TARAXA_NODE_PBFT_CHAIN_HPP
#define TARAXA_NODE_PBFT_CHAIN_HPP

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcore/db.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>

#include <iostream>
#include <string>
#include <vector>

#include "types.hpp"
#include "util.hpp"

/**
 * In pbft_chain, two kinds of blocks:
 * 1. PivotBlock: determine DAG block in pivot chain
 * 2. ScheduleBlock: determine sequential/concurrent set
 */
namespace taraxa {

class DbStorage;
class FullNode;
class Vote;

enum PbftVoteTypes {
  propose_vote_type = 0,
  soft_vote_type,
  cert_vote_type,
  next_vote_type
};

struct TrxSchedule {
 public:
  enum class TrxStatus : uint8_t { sequential = 1, parallel };

  TrxSchedule() = default;
  TrxSchedule(
      vec_blk_t const& blks,
      std::vector<std::vector<std::pair<trx_hash_t, uint>>> const& modes)
      : dag_blks_order(blks), trxs_mode(modes) {}
  explicit TrxSchedule(dev::RLP const& r);
  // Construct from RLP
  explicit TrxSchedule(bytes const& rlpData);
  ~TrxSchedule() {}

  // order of DAG blocks (in hash)
  vec_blk_t dag_blks_order;
  // It is multiple array of transactions
  // TODO: optimize trx_mode type
  std::vector<std::vector<std::pair<trx_hash_t, uint>>> trxs_mode;
  void streamRLP(dev::RLPStream& strm) const;
  bytes rlp() const;
  Json::Value getJson() const;
  void setSchedule(Json::Value const& tree);
  bool operator==(TrxSchedule const& other) const {
    return rlp() == other.rlp();
  }
  std::string getStr() const;
};
std::ostream& operator<<(std::ostream& strm, TrxSchedule const& trx_sche);

class PbftBlock {
 public:
  PbftBlock() = default;
  PbftBlock(blk_hash_t const& block_hash, uint64_t period)
      : PbftBlock(block_hash, blk_hash_t(0), TrxSchedule(), period, addr_t(0),
                  secret_t::random()) {}  // For unit test
  PbftBlock(blk_hash_t const& prev_blk_hash,
            blk_hash_t const& dag_blk_hash_as_pivot,
            TrxSchedule const& schedule, uint64_t period,
            addr_t const& beneficiary, secret_t const& sk);
  explicit PbftBlock(dev::RLP const& r);
  explicit PbftBlock(bytes const& b);

  explicit PbftBlock(std::string const& str);
  ~PbftBlock() {}

  blk_hash_t sha3(bool include_sig) const;
  std::string getJsonStr() const;
  Json::Value getJson() const;
  void streamRLP(dev::RLPStream& strm, bool include_sig) const;
  bytes rlp(bool include_sig) const;
  bool verifySig() const;  // TODO

  blk_hash_t getBlockHash() const;
  blk_hash_t getPrevBlockHash() const;
  blk_hash_t getPivotDagBlockHash() const;
  TrxSchedule getSchedule() const;
  uint64_t getPeriod() const;
  uint64_t getTimestamp() const;
  addr_t getBeneficiary() const;

 private:
  void calculateHash_();

  blk_hash_t block_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_as_pivot_;
  TrxSchedule schedule_;
  // PBFT head block is period 0, first PBFT block is period 1
  uint64_t period_;  // Block index
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t signature_;
};
std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk);

struct PbftBlockCert {
  PbftBlockCert(PbftBlock const& pbft_blk, std::vector<Vote> const& cert_votes);
  explicit PbftBlockCert(bytes const& all_rlp);
  PbftBlockCert(PbftBlock const& pbft_blk, bytes const& cert_votes_rlp);

  PbftBlock pbft_blk;
  std::vector<Vote> cert_votes;
  bytes rlp() const;
};
std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b);

class PbftChain {
 public:
  explicit PbftChain(std::string const& dag_genesis_hash, addr_t node_addr,
                     std::shared_ptr<DbStorage> db);
  virtual ~PbftChain() = default;

  void setPbftHead(std::string const& pbft_head_str);

  void cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block);

  uint64_t getPbftChainSize() const;
  blk_hash_t getHeadHash() const;
  blk_hash_t getLastPbftBlockHash() const;

  PbftBlock getPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  std::pair<PbftBlock, bool> getUnverifiedPbftBlock(
      blk_hash_t const& pbft_block_hash);
  std::vector<PbftBlock> getPbftBlocks(size_t period, size_t count) const;
  std::vector<std::string> getPbftBlocksStr(size_t period, size_t count,
                                            bool hash) const;
  std::string getHeadStr() const;
  std::string getJsonStr() const;
  void setLastPbftBlockHash(blk_hash_t const& new_pbft_block);

  bool findPbftBlockInChain(blk_hash_t const& pbft_block_hash) const;
  bool findUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash) const;
  bool findPbftBlockInSyncedSet(blk_hash_t const& pbft_block_hash) const;

  void pushUnverifiedPbftBlock(taraxa::PbftBlock const& pbft_block);
  void updatePbftChain(blk_hash_t const& pbft_block_hash);

  bool checkPbftBlockValidationFromSyncing(
      taraxa::PbftBlock const& pbft_block) const;
  bool checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const;

  uint64_t pbftSyncingPeriod() const;

  bool pbftSyncedQueueEmpty() const;
  PbftBlockCert pbftSyncedQueueFront() const;
  PbftBlockCert pbftSyncedQueueBack() const;
  void pbftSyncedQueuePopFront();
  void setSyncedPbftBlockIntoQueue(PbftBlockCert const& pbft_block_and_votes);
  void clearSyncedPbftBlocks();
  size_t pbftSyncedQueueSize() const;
  bool isKnownPbftBlockForSyncing(blk_hash_t const& pbft_block_hash) const;

 private:
  void pbftSyncedSetInsert_(blk_hash_t const& pbft_block_hash);
  void pbftSyncedSetErase_();
  void insertUnverifiedPbftBlockIntoParentMap_(
      blk_hash_t const& prev_block_hash, blk_hash_t const& block_hash);

  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  mutable boost::shared_mutex sync_access_;
  mutable boost::shared_mutex unverified_access_;

  blk_hash_t head_hash_;  // pbft head hash
  uint64_t size_;
  blk_hash_t last_pbft_block_hash_;

  blk_hash_t dag_genesis_hash_;  // dag genesis at height 1
  uint64_t max_dag_blocks_height_ = 0;

  std::shared_ptr<DbStorage> db_ = nullptr;

  // <prev block hash, vector<PBFT proposed blocks waiting for vote>>
  std::unordered_map<blk_hash_t, std::vector<blk_hash_t>>
      unverified_blocks_map_;
  std::unordered_map<blk_hash_t, PbftBlock> unverified_blocks_;

  // syncing pbft blocks from peers
  std::deque<PbftBlockCert> pbft_synced_queue_;
  std::unordered_set<blk_hash_t> pbft_synced_set_;

  LOG_OBJECTS_DEFINE;
};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

}  // namespace taraxa
#endif
