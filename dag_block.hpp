/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-10-31 16:26:37
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-16 13:09:42
 */

#ifndef DAG_BLOCKS_HPP
#define DAG_BLOCKS_HPP

#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include <boost/thread.hpp>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <string>
#include <thread>
#include "libdevcore/Log.h"
#include "transaction.hpp"
#include "types.hpp"
#include "util.hpp"

namespace taraxa {
using std::string;
class DagManager;
class Transaction;
class TransactionManager;
class FullNode;

// Block definition

/**
 * Note:
 * Need to sign first then sender() and hash() is available
 */

class DagBlock {
 public:
  DagBlock() = default;
  DagBlock(blk_hash_t pivot, vec_blk_t tips, vec_trx_t trxs, sig_t signature,
           blk_hash_t hash, addr_t sender);
  DagBlock(blk_hash_t pivot, vec_blk_t tips, vec_trx_t trxs);
  DagBlock(stream &strm);
  DagBlock(const string &json);
  DagBlock(dev::RLP const &_r);
  friend std::ostream &operator<<(std::ostream &str, DagBlock &u) {
    str << "	pivot		= " << u.pivot_.abridged() << std::endl;
    str << "	tips ( " << u.tips_.size() << " )	= ";
    for (auto const &t : u.tips_) str << t.abridged() << " ";
    str << std::endl;
    str << "	trxs ( " << u.trxs_.size() << " )	= ";
    for (auto const &t : u.trxs_) str << t.abridged() << " ";
    str << std::endl;
    str << "	signature	= " << u.sig_.abridged() << std::endl;
    str << "	hash		= " << u.hash_.abridged() << std::endl;
    str << "	sender		= " << u.cached_sender_.abridged() << std::endl;

    return str;
  }
  bool operator==(DagBlock const &other) const {
    return this->sha3(true) == other.sha3(true);
  }

  void serializeRLP(dev::RLPStream &s);
  blk_hash_t getPivot() const { return pivot_; }
  vec_blk_t getTips() const { return tips_; }
  vec_trx_t getTrxs() const { return trxs_; }
  sig_t getSig() const { return sig_; }
  blk_hash_t getHash() const { return hash_; }
  addr_t getSender() const { return sender(); }
  std::string getJsonStr() const;
  bool isValid() const;
  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);
  addr_t sender() const;
  void sign(secret_t const &sk);
  void updateHash() {
    if (!hash_) {
      hash_ = dev::sha3(rlp(true));
    }
  }
  bool verifySig() const;

 private:
  void streamRLP(dev::RLPStream &s, bool include_sig) const;
  bytes rlp(bool include_sig) const;
  blk_hash_t sha3(bool include_sig) const;
  blk_hash_t pivot_;
  vec_blk_t tips_;
  vec_trx_t trxs_;  // transactions
  sig_t sig_;
  blk_hash_t hash_;
  mutable addr_t cached_sender_;  // block creater
};

/**
 * Thread safe
 */

class BlockQueue {
 public:
  BlockQueue(size_t capacity, unsigned verify_threads);
  ~BlockQueue();
  void pushUnverifiedBlock(DagBlock const &block);  // add to unverified queue
  void pushUnverifiedBlock(
      DagBlock const &block,
      std::vector<Transaction> const &transactions);  // add to unverified queue
  std::pair<DagBlock, std::vector<Transaction> >
  getVerifiedBlock();  // get one verified block and pop
  void start();
  void stop();
  void setFullNode(std::shared_ptr<FullNode> node){ node_ = node;} 
  bool isBlockKnown(blk_hash_t const &hash);
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const &hash);

 private:
  using uLock = std::unique_lock<std::mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  void verifyBlock();
  bool stopped_ = true;
  size_t capacity_ = 2048;
  size_t num_verifiers_ = 1;
  mutable boost::shared_mutex
      shared_mutex_;          // shared mutex to check seen_blocks ...
  std::weak_ptr<FullNode> node_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  // seen blks
  std::map<blk_hash_t, DagBlock> seen_blocks_;

  std::vector<std::thread> verifiers_;
  mutable std::mutex mutex_for_unverified_qu_;
  mutable std::mutex mutex_for_verified_qu_;

  std::condition_variable cond_for_unverified_qu_;
  std::condition_variable cond_for_verified_qu_;

  std::deque<std::pair<DagBlock, std::vector<Transaction> > > unverified_qu_;
  std::deque<std::pair<DagBlock, std::vector<Transaction> > > verified_qu_;
  dev::Logger log_er_{
      dev::createLogger(dev::Verbosity::VerbosityError, "BLKQU")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "BLKQU")};
  dev::Logger log_nf_{
      dev::createLogger(dev::Verbosity::VerbosityInfo, "BLKQU")};
  dev::Logger log_dg_{
      dev::createLogger(dev::Verbosity::VerbosityDebug, "BLKQU")};
};

}  // namespace taraxa
#endif