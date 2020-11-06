#ifndef TARAXA_NODE_DAG_BLOCKS_HPP
#define TARAXA_NODE_DAG_BLOCKS_HPP

#include <libdevcore/CommonJS.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>

#include <atomic>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <string>
#include <thread>

#include "types.hpp"
#include "util.hpp"
#include "vdf_sortition.hpp"

namespace taraxa {
using std::string;
using VdfSortition = vdf_sortition::VdfSortition;
class DagManager;
class Transaction;
enum class TransactionStatus;
class TransactionManager;
class FullNode;
class DbStorage;
// Block definition

/**
 * Note:
 * Need to sign first then sender() and hash() is available
 */

class DagBlock {
  blk_hash_t pivot_;
  level_t level_ = 0;
  vec_blk_t tips_;
  vec_trx_t trxs_;  // transactions
  sig_t sig_;
  blk_hash_t hash_;
  uint64_t timestamp_ = 0;
  vdf_sortition::VdfSortition vdf_;
  mutable addr_t cached_sender_;  // block creater

 public:
  DagBlock() = default;
  DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, sig_t signature, blk_hash_t hash, addr_t sender);
  DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs);
  DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, VdfSortition const &vdf);
  explicit DagBlock(Json::Value const &doc);
  explicit DagBlock(string const &json);
  explicit DagBlock(dev::RLP const &_rlp);
  explicit DagBlock(dev::bytes const &_rlp) : DagBlock(dev::RLP(_rlp)) {}

  static std::vector<trx_hash_t> extract_transactions_from_rlp(dev::RLP const &rlp);

  friend std::ostream &operator<<(std::ostream &str, DagBlock const &u) {
    str << "	pivot		= " << u.pivot_.abridged() << std::endl;
    str << "	level		= " << u.level_ << std::endl;
    str << "	tips ( " << u.tips_.size() << " )	= ";
    for (auto const &t : u.tips_) str << t.abridged() << " ";
    str << std::endl;
    str << "	trxs ( " << u.trxs_.size() << " )	= ";
    for (auto const &t : u.trxs_) str << t.abridged() << " ";
    str << std::endl;
    str << "	signature	= " << u.sig_.abridged() << std::endl;
    str << "	hash		= " << u.hash_.abridged() << std::endl;
    str << "	sender		= " << u.cached_sender_.abridged() << std::endl;
    str << "  vdf = " << u.vdf_ << std::endl;
    return str;
  }
  bool operator==(DagBlock const &other) const { return this->sha3(true) == other.sha3(true); }

  auto const &getPivot() const { return pivot_; }
  auto getLevel() const { return level_; }
  auto getTimestamp() const { return timestamp_; }
  auto const &getTips() const { return tips_; }
  auto const &getTrxs() const { return trxs_; }
  auto const &getSig() const { return sig_; }
  auto const &getHash() const { return hash_; }
  auto const &getVdf() const { return vdf_; }

  addr_t getSender() const { return sender(); }
  Json::Value getJson() const;
  std::string getJsonStr() const;
  bool isValid() const;
  addr_t sender() const;
  void sign(secret_t const &sk);
  void updateHash() {
    if (!hash_) {
      hash_ = dev::sha3(rlp(true));
    }
  }
  bool verifySig() const;
  bytes rlp(bool include_sig) const;

 private:
  void streamRLP(dev::RLPStream &s, bool include_sig) const;
  blk_hash_t sha3(bool include_sig) const;
};

enum class BlockStatus { invalid, proposed, broadcasted, verified, unseen };

using BlockStatusTable = ExpirationCacheMap<blk_hash_t, BlockStatus>;

struct DagFrontier {
  DagFrontier() = default;
  DagFrontier(blk_hash_t const &pivot, vec_blk_t const &tips) : pivot(pivot), tips(tips) {}
  void clear() {
    pivot.clear();
    tips.clear();
  }
  blk_hash_t pivot;
  vec_blk_t tips;
};

/**
 * Thread safe
 */
class BlockManager {
 public:
  BlockManager(size_t capacity, unsigned verify_threads, addr_t node_addr, std::shared_ptr<DbStorage> db, std::shared_ptr<TransactionManager> trx_mgr,
               boost::log::sources::severity_channel_logger<> log_time_, uint32_t queue_limit = 0);
  ~BlockManager();
  void insertBlock(DagBlock const &blk);
  // Only used in initial syncs when blocks are received with full list of
  // transactions
  void insertBroadcastedBlockWithTransactions(DagBlock const &blk, std::vector<Transaction> const &transactions);
  void pushUnverifiedBlock(DagBlock const &block,
                           bool critical);  // add to unverified queue
  void pushUnverifiedBlock(DagBlock const &block, std::vector<Transaction> const &transactions,
                           bool critical);  // add to unverified queue
  DagBlock popVerifiedBlock();              // get one verified block and pop
  void pushVerifiedBlock(DagBlock const &blk);
  std::pair<size_t, size_t> getDagBlockQueueSize() const;
  level_t getMaxDagLevelInQueue() const;
  void start();
  void stop();
  bool isBlockKnown(blk_hash_t const &hash);
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const &hash) const;
  void clearBlockStatausTable() { blk_status_.clear(); }
  bool pivotAndTipsValid(DagBlock const &blk);

 private:
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  void verifyBlock();
  std::atomic<bool> stopped_ = true;
  size_t capacity_ = 2048;
  size_t num_verifiers_ = 4;

  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<DbStorage> db_;
  boost::log::sources::severity_channel_logger<> log_time_;
  // seen blks
  BlockStatusTable blk_status_;
  ExpirationCacheMap<blk_hash_t, DagBlock> seen_blocks_;
  mutable boost::shared_mutex shared_mutex_;  // shared mutex to check seen_blocks ...
  std::vector<std::thread> verifiers_;
  mutable boost::shared_mutex shared_mutex_for_unverified_qu_;
  mutable boost::shared_mutex shared_mutex_for_verified_qu_;

  boost::condition_variable_any cond_for_unverified_qu_;
  boost::condition_variable_any cond_for_verified_qu_;
  uint32_t queue_limit_;

  std::map<uint64_t, std::deque<std::pair<DagBlock, std::vector<Transaction> > > > unverified_qu_;
  std::map<uint64_t, std::deque<DagBlock> > verified_qu_;

  LOG_OBJECTS_DEFINE;
};

}  // namespace taraxa
#endif