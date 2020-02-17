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
using boost::property_tree::ptree;

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
  TrxSchedule(dev::RLP const& r);
  // Construct from RLP
  TrxSchedule(bytes const& rlpData);
  ~TrxSchedule() {}

  // order of DAG blocks (in hash)
  vec_blk_t dag_blks_order;
  // It is multiple array of transactions
  // TODO: optimize trx_mode type
  std::vector<std::vector<std::pair<trx_hash_t, uint>>> trxs_mode;
  void streamRLP(dev::RLPStream& strm) const;
  bytes rlp() const;
  void setPtree(ptree& tree) const;
  void setSchedule(ptree const& tree);
  bool operator==(TrxSchedule const& other) const {
    return rlp() == other.rlp();
  }
  std::string getStr() const;
};
std::ostream& operator<<(std::ostream& strm, TrxSchedule const& trx_sche);

class PbftBlock {
 public:
  PbftBlock() = default;
  PbftBlock(blk_hash_t const& block_hash, uint64_t height)
      : block_hash_(block_hash), height_(height) {}  // For unit test
  PbftBlock(blk_hash_t const& prev_blk_hash,
            blk_hash_t const& dag_blk_hash_as_pivot,
            TrxSchedule const& schedule, uint64_t period, uint64_t height,
            uint64_t timestamp, addr_t const& beneficiary)
      : prev_block_hash_(prev_blk_hash),
        dag_block_hash_as_pivot_(dag_blk_hash_as_pivot),
        schedule_(schedule),
        period_(period),
        height_(height),
        timestamp_(timestamp),
        beneficiary_(beneficiary) {}
  PbftBlock(dev::RLP const& r);
  PbftBlock(bytes const& b);

  PbftBlock(std::string const& str);
  ~PbftBlock() {}

  std::string getJsonStr() const;
  addr_t getBeneficiary() const;
  blk_hash_t sha3(bool include_sig) const;
  void sign(secret_t const& sk);
  void calculateHash();
  bool verifySig() const;
  void streamRLP(dev::RLPStream& strm, bool include_sig) const;
  bytes rlp(bool include_sig) const;

  blk_hash_t getBlockHash() const { return block_hash_; }
  blk_hash_t getPrevBlockHash() const { return prev_block_hash_; }
  blk_hash_t getPivotDagBlockHash() const { return dag_block_hash_as_pivot_; }
  TrxSchedule getSchedule() const { return schedule_; }
  uint64_t getPeriod() const { return period_; }
  uint64_t getHeight() const { return height_; }
  uint64_t getTimestamp() const { return timestamp_; }

 private:
  blk_hash_t block_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_as_pivot_;
  TrxSchedule schedule_;
  uint64_t
      period_;  // PBFT head block is period 0, first PBFT block is period 1
  uint64_t
      height_;  // PBFT head block is height 1, first PBFT blick is height 2
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t signature_;
};
std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk);

struct PbftBlockCert {
  PbftBlockCert(PbftBlock const& pbft_blk, std::vector<Vote> const& cert_votes);
  PbftBlockCert(bytes const& all_rlp);
  PbftBlockCert(PbftBlock const& pbft_blk, bytes const& cert_votes_rlp);

  PbftBlock pbft_blk;
  std::vector<Vote> cert_votes;
  bytes rlp() const;
};
std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b);

class PbftChain {
 public:
  PbftChain(std::string const& dag_genesis_hash);
  virtual ~PbftChain() = default;

  void setFullNode(std::shared_ptr<FullNode> node);
  void setPbftGenesis(std::string const& pbft_genesis_str);

  void cleanupUnverifiedPbftBlocks(taraxa::PbftBlock const& pbft_block);

  uint64_t getPbftChainSize() const { return size_; }
  uint64_t getPbftChainPeriod() const { return period_; }
  blk_hash_t getGenesisHash() const { return genesis_hash_; }
  blk_hash_t getLastPbftBlockHash() const { return last_pbft_block_hash_; }

  PbftBlock getPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  std::pair<PbftBlock, bool> getUnverifiedPbftBlock(
      blk_hash_t const& pbft_block_hash);
  std::vector<PbftBlock> getPbftBlocks(size_t height, size_t count) const;
  std::vector<std::string> getPbftBlocksStr(size_t height, size_t count,
                                            bool hash) const;
  std::string getGenesisStr() const;
  std::string getJsonStr() const;
  std::pair<blk_hash_t, bool> getDagBlockHash(uint64_t dag_block_height) const;
  std::pair<uint64_t, bool> getDagBlockHeight(
      blk_hash_t const& dag_block_hash) const;
  uint64_t getDagBlockMaxHeight() const;

  void setLastPbftBlockHash(blk_hash_t const& new_pbft_block);

  bool findPbftBlockInChain(blk_hash_t const& pbft_block_hash) const;
  bool findUnverifiedPbftBlock(blk_hash_t const& pbft_block_hash) const;
  bool findPbftBlockInSyncedSet(blk_hash_t const& pbft_block_hash) const;

  bool pushPbftBlockIntoChain(taraxa::PbftBlock const& pbft_block);
  bool pushPbftBlock(taraxa::PbftBlock const& pbft_block);
  void pushUnverifiedPbftBlock(taraxa::PbftBlock const& pbft_block);
  uint64_t pushDagBlockHash(blk_hash_t const& dag_block_hash);

  bool checkPbftBlockValidationFromSyncing(
      taraxa::PbftBlock const& pbft_block) const;
  bool checkPbftBlockValidation(taraxa::PbftBlock const& pbft_block) const;

  uint64_t pbftSyncingHeight() const;

  bool pbftSyncedQueueEmpty() const;
  PbftBlockCert pbftSyncedQueueFront() const;
  PbftBlockCert pbftSyncedQueueBack() const;
  void pbftSyncedQueuePopFront();
  void setSyncedPbftBlockIntoQueue(PbftBlockCert const& pbft_block_and_votes);
  void clearSyncedPbftBlocks();
  size_t pbftSyncedQueueSize() const;

 private:
  void pbftSyncedSetInsert_(blk_hash_t const& pbft_block_hash);
  void pbftSyncedSetErase_();
  void insertPbftBlockIndex_(blk_hash_t const& pbft_block_hash);
  void insertUnverifiedPbftBlockIntoParentMap_(
      blk_hash_t const& prev_block_hash, blk_hash_t const& block_hash);

  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  mutable boost::shared_mutex sync_access_;
  mutable boost::shared_mutex unverified_access_;

  blk_hash_t genesis_hash_;  // pbft chain head hash
  uint64_t size_;    // PBFT head with size 1, first PBFT block with size 2
  uint64_t period_;  // PBFT head with period 0, first PBFT block with period 1
  blk_hash_t last_pbft_block_hash_;

  blk_hash_t dag_genesis_hash_;  // dag genesis at height 1
  uint64_t max_dag_blocks_height_ = 0;

  std::weak_ptr<FullNode> node_;
  std::shared_ptr<DbStorage> db_ = nullptr;

  // <prev block hash, vector<PBFT proposed blocks waiting for vote>>
  std::unordered_map<blk_hash_t, std::vector<blk_hash_t>>
      unverified_blocks_map_;
  std::unordered_map<blk_hash_t, PbftBlock> unverified_blocks_;

  // syncing pbft blocks from peers
  std::deque<PbftBlockCert> pbft_synced_queue_;
  std::unordered_set<blk_hash_t> pbft_synced_set_;

  mutable dev::Logger log_sil_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "PBFT_CHAIN")};
  mutable dev::Logger log_err_{
      dev::createLogger(dev::Verbosity::VerbosityError, "PBFT_CHAIN")};
  mutable dev::Logger log_war_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "PBFT_CHAIN")};
  mutable dev::Logger log_inf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "PBFT_CHAIN")};
  mutable dev::Logger log_deb_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "PBFT_CHAIN")};
  mutable dev::Logger log_tra_{
      dev::createLogger(dev::Verbosity::VerbosityTrace, "PBFT_CHAIN")};
};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

}  // namespace taraxa
#endif
