/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:06
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-01
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

#include "pbft_manager.hpp"
#include "types.hpp"
#include "util.hpp"

/**
 * In pbft_chain, three kinds of blocks:
 * 1. PivotBlock: pivot chain
 * 2. ScheduleBlock: determines concurrent set
 * 3. ResultBlock: computing results
 */
namespace taraxa {

struct TrxSchedule {
  enum class TrxStatus : uint8_t { invalid, sequential, parallel };
  // TrxSchedule()=default;

  explicit TrxSchedule(vec_blk_t const& blks,
                       std::vector<std::vector<uint>> const& modes)
      : blk_order(blks), vec_trx_modes(modes) {}
  // Construct from RLP
  explicit TrxSchedule(bytes const& rlpData);
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
  PivotBlock(blk_hash_t const& pivot_blk,
             blk_hash_t const& prev_pivot_blk,
             blk_hash_t const& prev_res_blk,
             blk_hash_t const& dag_blk,
             uint64_t epoch,
             uint64_t timestamp,
             addr_t const& beneficiary,
             sig_t const& sig) : pivot_blk_(pivot_blk),
                                 prev_pivot_blk_(prev_pivot_blk),
                                 prev_res_blk_(prev_res_blk),
                                 dag_blk_(dag_blk),
                                 epoch_(epoch),
                                 timestamp_(timestamp),
                                 beneficiary_(beneficiary),
                                 sig_(sig) {}
  ~PivotBlock() {}

  blk_hash_t getHash() const;
  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);

  friend std::ostream &operator<<(std::ostream &strm,
                                  PivotBlock const& pivot_block) {
    strm << "[Pivot Block] " << std::endl;
    strm << "  pivot hash: " << pivot_block.pivot_blk_.hex() << std::endl;
    strm << "  previous pivot hash: "
         << pivot_block.prev_pivot_blk_.hex() << std::endl;
    strm << "  previous result hash: "
         << pivot_block.prev_res_blk_.hex() << std::endl;
    strm << "  dag hash: " << pivot_block.dag_blk_.hex() << std::endl;
    strm << "  epoch: " << pivot_block.epoch_ << std::endl;
    strm << "  timestamp: " << pivot_block.timestamp_ << std::endl;
    strm << "  beneficiary: " << pivot_block.beneficiary_.hex() << std::endl;
    strm << "  signature: " << pivot_block.sig_.hex() << std::endl;
    return strm;
  }

 private:
  blk_hash_t pivot_blk_;
  blk_hash_t prev_pivot_blk_;
  blk_hash_t prev_res_blk_;
  blk_hash_t dag_blk_;
  uint64_t epoch_;
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t sig_;
};

class ScheduleBlock {
 public:
  ScheduleBlock(blk_hash_t const& prev_pivot, uint64_t const& timestamp,
                sig_t const& sig, TrxSchedule const& sche)
      : prev_pivot_(prev_pivot),
        timestamp_(timestamp),
        sig_(sig),
        schedule_(sche) {}
  ScheduleBlock(dev::RLP const &rlp);
  ~ScheduleBlock() {}

  std::string getJsonStr() const;
  TrxSchedule getSchedule() const {return schedule_;}
  blk_hash_t getHash() const;

  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);

  friend std::ostream;

 private:
  blk_hash_t schedule_blk_;
  blk_hash_t prev_pivot_;
  uint64_t timestamp_;
  sig_t sig_;
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
  PbftBlock(dev::RLP const &_r);
  ~PbftBlock() {}

  PbftBlockTypes getBlockType() const { return block_type_; }
  void setBlockType(PbftBlockTypes block_type) { block_type_ = block_type; }

  PivotBlock getPivotBlock() const { return pivot_block_; }
  blk_hash_t getHash() const;
  void setPivotBlock(PivotBlock &pivot_block) { pivot_block_ = pivot_block; }

  void serializeRLP(dev::RLPStream &s) const;
  bool serialize(stream &strm) const;
  bool deserialize(stream &strm);

 private:
  PbftBlockTypes block_type_ = pbft_block_none_type;
  PivotBlock pivot_block_;
  // TODO: need more pbft block type
};

class PbftChain {
 public:
  PbftChain() = default;
  PbftChain(blk_hash_t first_pivot_blk)
    : count(2),
      is_pbft_genesis_(false),
      next_pbft_block_type_(schedule_block_type),
      last_pbft_blk_(first_pivot_blk) {}
  ~PbftChain() {}

  size_t getSize() const { return count; }
  blk_hash_t getLastPbftBlock() const;
  bool isPbftGenesis() const;
  void setNotPbftGenesis();
  void setLastPbftBlock(blk_hash_t &new_pbft_block);
  void setNextPbftBlockType(PbftBlockTypes next_block_type);

  bool findPbftBlock(blk_hash_t const& pbft_block_hash);
  PbftBlock getPbftBlock(blk_hash_t &pbft_block_hash);
  void insertPbftBlock(blk_hash_t &pbft_block_hash, PbftBlock &pbft_block);
  void pushPbftBlock(taraxa::PbftBlock &pbft_block);

 private:
  size_t count = 1;
  bool is_pbft_genesis_ = true;
  PbftBlockTypes next_pbft_block_type_ = pivot_block_type;
  blk_hash_t last_pbft_blk_;
  std::unordered_map<blk_hash_t, PbftBlock> pbft_blocks_map_;
};

}  // namespace taraxa
#endif
