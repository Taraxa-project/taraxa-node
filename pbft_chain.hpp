/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:06
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-08 18:55:04
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
 private:
  blk_hash_t prev_res_blk_;
  blk_hash_t prev_pivot_blk_;
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
  std::string getJsonStr() const;
  TrxSchedule getSchedule() const {return schedule_;}
  friend std::ostream;

 private:
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
}  // namespace taraxa
#endif
