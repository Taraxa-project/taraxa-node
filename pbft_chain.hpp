#ifndef TARAXA_NODE_PBFT_CHAIN_HPP
#define TARAXA_NODE_PBFT_CHAIN_HPP

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include <iostream>
#include <string>
#include <vector>

#include "simple_db_face.hpp"
#include "types.hpp"
#include "util.hpp"

/**
 * In pbft_chain, three kinds of blocks:
 * 1. PivotBlock: pivot chain
 * 2. ScheduleBlock: determines concurrent set
 * 3. ResultBlock: computing results
 */
namespace taraxa {
using boost::property_tree::ptree;

enum PbftBlockTypes {
  pbft_block_none_type = -1,
  pivot_block_type = 0,
  schedule_block_type,
  result_block_type
};

enum PbftVoteTypes {
  propose_vote_type = 0,
  soft_vote_type,
  cert_vote_type,
  next_vote_type
};

struct TrxSchedule {
 public:
  enum class TrxStatus : uint8_t { invalid, sequential, parallel };

  TrxSchedule() = default;
  TrxSchedule(vec_blk_t const& blks,
              std::vector<std::vector<uint>> const& modes)
      : blk_order(blks), vec_trx_modes(modes) {}
  // Construct from RLP
  TrxSchedule(bytes const& rlpData);
  ~TrxSchedule() {}

  bytes rlp() const;
  // order of blocks (in hash)
  vec_blk_t blk_order;
  // It is multiple array of transactions
  // TODO: optimize trx_mode type
  std::vector<std::vector<uint>> vec_trx_modes;
  std::string getJsonStr() const;
  bool operator==(TrxSchedule const& other) const {
    return rlp() == other.rlp();
  }
};
std::ostream& operator<<(std::ostream& strm, TrxSchedule const& tr_sche);

class FullNode;

class PivotBlock {
 public:
  PivotBlock() = default;
  PivotBlock(taraxa::stream& strm);
  PivotBlock(std::string const& json);

  PivotBlock(blk_hash_t const& prev_pivot_hash,
             blk_hash_t const& prev_block_hash,
             blk_hash_t const& dag_block_hash, uint64_t period,
             addr_t beneficiary)
      : prev_pivot_hash_(prev_pivot_hash),
        prev_block_hash_(prev_block_hash),
        dag_block_hash_(dag_block_hash),
        period_(period),
        beneficiary_(beneficiary) {}
  ~PivotBlock() {}

  blk_hash_t getPrevPivotBlockHash() const;
  blk_hash_t getPrevBlockHash() const;
  blk_hash_t getDagBlockHash() const;
  uint64_t getPeriod() const;
  addr_t getBeneficiary() const;

  void setJsonTree(ptree& tree) const;
  void setBlockByJson(ptree const& doc);

  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);
  void streamRLP(dev::RLPStream& strm) const;

  friend std::ostream& operator<<(std::ostream& strm,
                                  PivotBlock const& pivot_block) {
    strm << "[Pivot Block]" << std::endl;
    strm << "  previous pivot hash: " << pivot_block.prev_pivot_hash_.hex()
         << std::endl;
    strm << "  previous result hash: " << pivot_block.prev_block_hash_.hex()
         << std::endl;
    strm << "  dag hash: " << pivot_block.dag_block_hash_.hex() << std::endl;
    strm << "  period: " << pivot_block.period_ << std::endl;
    strm << "  beneficiary: " << pivot_block.beneficiary_.hex() << std::endl;
    return strm;
  }

 private:
  blk_hash_t prev_pivot_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_;
  uint64_t period_;
  addr_t beneficiary_;
};

class ScheduleBlock {
 public:
  ScheduleBlock() = default;
  ScheduleBlock(blk_hash_t const& prev_block_hash, TrxSchedule const& schedule)
      : prev_block_hash_(prev_block_hash), schedule_(schedule) {}
  ScheduleBlock(taraxa::stream& strm);
  ~ScheduleBlock() {}
  void streamRLP(dev::RLPStream& strm) const;

  std::string getJsonStr() const;
  Json::Value getJson() const;
  TrxSchedule getSchedule() const;
  blk_hash_t getPrevBlockHash() const;

  void setJsonTree(ptree& tree) const;
  void setBlockByJson(ptree const& json);

  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);

  friend std::ostream;

 private:
  blk_hash_t prev_block_hash_;
  TrxSchedule schedule_;
};
std::ostream& operator<<(std::ostream& strm, ScheduleBlock const& sche_blk);

class ResultBlock {
 public:
 private:
  val_t state_root_;
  val_t trx_root_;
  val_t receipt_root_;
  val_t gas_limit_;
  val_t gas_used_;
};

class PbftBlock {
 public:
  PbftBlock() = default;
  PbftBlock(blk_hash_t const& block_hash) : block_hash_(block_hash) {}
  PbftBlock(PivotBlock const& pivot_block)
      : pivot_block_(pivot_block), block_type_(pivot_block_type) {}
  PbftBlock(ScheduleBlock const& schedule_block)
      : schedule_block_(schedule_block), block_type_(schedule_block_type) {}
  PbftBlock(dev::RLP const& _r);
  PbftBlock(std::string const& json);
  ~PbftBlock() {}

  blk_hash_t getBlockHash() const;
  PbftBlockTypes getBlockType() const;
  PivotBlock getPivotBlock() const;
  ScheduleBlock getScheduleBlock() const;
  uint64_t getTimestamp() const;
  std::string getJsonStr() const;

  void setBlockHash();
  void setBlockType(PbftBlockTypes block_type);
  void setPivotBlock(PivotBlock const& pivot_block);
  void setScheduleBlock(ScheduleBlock const& schedule_block);
  void setTimestamp(uint64_t const timestamp);
  void setSignature(sig_t const& signature);

  void serializeRLP(dev::RLPStream& s) const;
  bool serialize(stream& strm) const;
  bool deserialize(stream& strm);
  void streamRLP(dev::RLPStream& strm) const;

 private:
  blk_hash_t block_hash_;
  PbftBlockTypes block_type_ = pbft_block_none_type;
  PivotBlock pivot_block_;
  ScheduleBlock schedule_block_;
  uint64_t timestamp_;
  sig_t signature_;
  // TODO: need more pbft block type
};
std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk);

class PbftChain {
 public:
  PbftChain(std::string const &dag_genesis_hash);
  ~PbftChain() {}

  void setFullNode(std::shared_ptr<FullNode> full_node);
  void releaseDB() { db_pbftchain_ = nullptr; }

  uint64_t getPbftChainSize() const;
  uint64_t getPbftChainPeriod() const;
  blk_hash_t getGenesisHash() const;
  blk_hash_t getLastPbftBlockHash() const;
  blk_hash_t getLastPbftPivotHash() const;
  PbftBlockTypes getNextPbftBlockType() const;
  size_t getPbftUnverifiedQueueSize() const;
  PbftBlock getPbftBlockInChain(blk_hash_t const& pbft_block_hash);
  std::pair<PbftBlock, bool> getPbftBlockInQueue(
      blk_hash_t const& pbft_block_hash);
  std::vector<PbftBlock> getPbftBlocks(size_t height, size_t count) const;
  std::string getGenesisStr() const;
  std::string getJsonStr() const;
  dev::Logger& getLoggerErr() { return log_err_; }
  std::pair<blk_hash_t, bool> getDagBlockHash(uint64_t dag_block_height) const;
  std::pair<uint64_t, bool> getDagBlockHeight(
      blk_hash_t const& dag_block_hash) const;
  uint64_t getDagBlockMaxHeight() const;

  void setLastPbftBlockHash(blk_hash_t const& new_pbft_block);
  void setNextPbftBlockType(PbftBlockTypes next_block_type);  // Test only

  bool findPbftBlockInChain(blk_hash_t const& pbft_block_hash) const;
  bool findPbftBlockInQueue(blk_hash_t const& pbft_block_hash) const;
  bool findPbftBlockInVerifiedSet(blk_hash_t const& pbft_block_hash) const;

  bool pushPbftBlockIntoChain(taraxa::PbftBlock const& pbft_block);
  bool pushPbftBlock(taraxa::PbftBlock const& pbft_block);
  bool pushPbftPivotBlock(taraxa::PbftBlock const& pbft_block);
  bool pushPbftScheduleBlock(taraxa::PbftBlock const& pbft_block);
  void pushPbftBlockIntoQueue(taraxa::PbftBlock const& pbft_block);
  uint64_t pushDagBlockHash(blk_hash_t const& dag_block_hash);

  void removePbftBlockInQueue(blk_hash_t const& block_hash);

  size_t pbftVerifiedSetSize() const;
  void pbftVerifiedSetInsert_(blk_hash_t const& pbft_block_hash);
  bool pbftVerifiedQueueEmpty() const;
  PbftBlock pbftVerifiedQueueFront() const;
  void pbftVerifiedQueuePopFront();
  void setVerifiedPbftBlockIntoQueue(PbftBlock const& pbft_block);

  // only for test
  void cleanPbftQueue() { pbft_unverified_queue_.clear(); }

 private:
  void insertPbftBlockIndex_(blk_hash_t const& pbft_block_hash);

  using uniqueLock_ = boost::unique_lock<boost::shared_mutex>;
  using sharedLock_ = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock_ = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock_ = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  mutable boost::shared_mutex access_;

  blk_hash_t genesis_hash_;
  uint64_t size_;
  uint64_t period_;
  PbftBlockTypes next_pbft_block_type_;
  blk_hash_t last_pbft_block_hash_;
  blk_hash_t last_pbft_pivot_hash_;

  std::weak_ptr<FullNode> node_;
  std::shared_ptr<SimpleDBFace> db_pbftchain_ = nullptr;

  // TODO: Need to think of how to shrink these info(by using LRU cache?), or
  //  move to DB
  std::vector<blk_hash_t> pbft_blocks_index_;
  std::deque<blk_hash_t> pbft_unverified_queue_;  // TODO: may not need it
  std::unordered_map<blk_hash_t, PbftBlock> pbft_unverified_map_;
  std::vector<blk_hash_t> dag_blocks_order_;  // DAG genesis at index 0
  // map<dag_block_hash, block_number> DAG genesis is block height 0
  std::unordered_map<blk_hash_t, uint64_t> dag_blocks_map_;
  // syncing pbft blocks from peers
  std::deque<PbftBlock> pbft_verified_queue_;
  std::unordered_set<blk_hash_t> pbft_verified_set_;

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
