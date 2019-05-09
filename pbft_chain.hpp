/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:06
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-07
 */
#ifndef PBFT_CHAIN_HPP
#define PBFT_CHAIN_HPP

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
#include <iostream>
#include <string>
#include <vector>

#include "types.hpp"
#include "util.hpp"

/**
 * In pbft_chain, three kinds of blocks:
 * 1. PivotBlock: pivot chain
 * 2. ScheduleBlock: determines concurrent set
 * 3. ResultBlock: computing results
 */
namespace taraxa {

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

class TrxSchedule {
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

class PivotBlock {
 public:
  PivotBlock() = default;
  PivotBlock(taraxa::stream &strm);

  PivotBlock(blk_hash_t const& prev_pivot_hash,
             blk_hash_t const& prev_block_hash,
             blk_hash_t const& dag_block_hash,
             uint64_t epoch,
             uint64_t timestamp,
             addr_t beneficiary) : prev_pivot_hash_(prev_pivot_hash),
                                   prev_block_hash_(prev_block_hash),
                                   dag_block_hash_(dag_block_hash),
                                   epoch_(epoch),
                                   timestamp_(timestamp),
                                   beneficiary_(beneficiary) {}
  ~PivotBlock() {}

  blk_hash_t getPrevPivotBlockHash() const;
  blk_hash_t getPrevBlockHash() const;
  blk_hash_t getDagBlockHash() const;
  uint64_t getEpoch() const;
  uint64_t getTimestamp() const;
  addr_t getBeneficiary() const;

  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);
  void streamRLP(dev::RLPStream &strm) const;


  friend std::ostream &operator<<(std::ostream &strm,
                                  PivotBlock const& pivot_block) {
    strm << "[Pivot Block] " << std::endl;
    strm << "  previous pivot hash: "
         << pivot_block.prev_pivot_hash_.hex() << std::endl;
    strm << "  previous result hash: "
         << pivot_block.prev_block_hash_.hex() << std::endl;
    strm << "  dag hash: " << pivot_block.dag_block_hash_.hex() << std::endl;
    strm << "  epoch: " << pivot_block.epoch_ << std::endl;
    strm << "  timestamp: " << pivot_block.timestamp_ << std::endl;
    strm << "  beneficiary: " << pivot_block.beneficiary_.hex() << std::endl;
    return strm;
  }

 private:
  blk_hash_t prev_pivot_hash_;
  blk_hash_t prev_block_hash_;
  blk_hash_t dag_block_hash_;
  uint64_t epoch_;
  uint64_t timestamp_;
  addr_t beneficiary_;
};

class ScheduleBlock {
 public:
  ScheduleBlock() = default;
  ScheduleBlock(blk_hash_t const& prev_block_hash,
                uint64_t const& timestamp,
                TrxSchedule const& schedule)
      : prev_block_hash_(prev_block_hash),
        timestamp_(timestamp),
        schedule_(schedule) {}
  ScheduleBlock(taraxa::stream &strm);
  ~ScheduleBlock() {}
  void streamRLP(dev::RLPStream &strm) const;

  std::string getJsonStr() const;
  TrxSchedule getSchedule() const;
  blk_hash_t getPrevBlockHash() const;

  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);

  friend std::ostream;

 private:
  blk_hash_t prev_block_hash_;
  uint64_t timestamp_;
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
  PbftBlock(blk_hash_t const &block_hash) : block_hash_(block_hash) {}
  PbftBlock(PivotBlock const &pivot_block) : pivot_block_(pivot_block),
                                             block_type_(pivot_block_type) {}
  PbftBlock(ScheduleBlock const &schedule_block)
    : schedule_block_(schedule_block), block_type_(schedule_block_type) {}
  PbftBlock(dev::RLP const &_r);
  ~PbftBlock() {}


  blk_hash_t getBlockHash() const;
  PbftBlockTypes getBlockType() const;
  PivotBlock getPivotBlock() const;
  ScheduleBlock getScheduleBlock() const;

  void setBlockHash();
  void setBlockType(PbftBlockTypes block_type);
  void setPivotBlock(PivotBlock const& pivot_block);
  void setScheduleBlock(ScheduleBlock const& schedule_block);
  void setSignature(sig_t const& signature);

  void serializeRLP(dev::RLPStream &s) const;
  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);
  void streamRLP(dev::RLPStream &strm) const;

 private:
  blk_hash_t block_hash_;
  PbftBlockTypes block_type_ = pbft_block_none_type;
  PivotBlock pivot_block_;
  ScheduleBlock schedule_block_;
  sig_t signature_;
  // TODO: need more pbft block type
};

class PbftChain {
 public:
  PbftChain() : genesis_hash_(blk_hash_t(0)),
                count_(1),
                next_pbft_block_type_(pivot_block_type) {
    last_pbft_block_hash_ = genesis_hash_;
    last_pbft_pivot_hash_ = genesis_hash_;
    pbft_blocks_map_[genesis_hash_] = PbftBlock(blk_hash_t(0));
  }
  ~PbftChain() {}

  size_t getSize() const;
  blk_hash_t getLastPbftBlockHash() const;
  blk_hash_t getLastPbftPivotHash() const;
  PbftBlockTypes getNextPbftBlockType() const;
  size_t getPbftQueueSize() const;
  std::pair<PbftBlock, bool> getPbftBlock(blk_hash_t const& pbft_block_hash);
  std::string getGenesisStr() const;

  void setLastPbftBlockHash(blk_hash_t const& new_pbft_block);
  void setNextPbftBlockType(PbftBlockTypes next_block_type);

  bool findPbftBlock(blk_hash_t const& pbft_block_hash) const ;
  bool findPbftBlockInQueue(blk_hash_t const& pbft_block_hash) const;

  void pushPbftBlock(taraxa::PbftBlock const& pbft_block);
  bool pushPbftPivotBlock(taraxa::PbftBlock const& pbft_block);
  bool pushPbftScheduleBlock(taraxa::PbftBlock const& pbft_block);
  void pushPbftBlockIntoQueue(taraxa::PbftBlock const& pbft_block);

 private:
  void insertPbftBlock_(blk_hash_t const& pbft_block_hash,
	                     PbftBlock const& pbft_block);

  blk_hash_t genesis_hash_;
  uint64_t count_;
  PbftBlockTypes next_pbft_block_type_;
  blk_hash_t last_pbft_block_hash_;
  blk_hash_t last_pbft_pivot_hash_;
  std::unordered_map<blk_hash_t, PbftBlock> pbft_blocks_map_;
  std::deque<blk_hash_t> pbft_queue_;
  std::unordered_map<blk_hash_t, PbftBlock> pbft_queue_map_;

};
std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain);

}  // namespace taraxa
#endif
