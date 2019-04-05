/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:06
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-20 22:23:40
 */
#pragma once
#include <iostream>
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
  std::vector<vec_trx_t> schedules;
};

class PivotBlock {
 public:
 private:
  blk_hash_t prev_res_blk_;
  blk_hash_t prev_pivot_blk_;
  uint64_t epoch_;
  uint64_t timestamp_;
  addr_t beneficiary_;
  sig_t sig_;
};

class ScheduleBlock {
 public:
 private:
  uint64_t prev_pivot_blk_;
  uint64_t timestamp_;
  sig_t sig_;
  TrxSchedule schedule_;
};

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
