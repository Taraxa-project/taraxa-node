/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:06
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-08 16:29:45
 */
#pragma once
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
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
  enum class TrxStatus : uint8_t { invalid, sequential, parallel };
  bytes rlp() const {
    dev::RLPStream s;
    auto num_blk = blk_order.size();
    std::vector<size_t> trx_in_each_blk;
    size_t num_trx = 0;
    for (auto i = 0; i < vec_trx_modes.size(); ++i) {
      trx_in_each_blk.emplace_back(vec_trx_modes[i].size());
      num_trx += vec_trx_modes[i].size();
    }
    s.appendList(1 + num_blk + num_blk + num_trx);

    // write number of blks
    s << num_blk;
    // write blk hashes
    for (auto i = 0; i < num_blk; ++i) {
      s << blk_order[i];
    }
    // write trx info
    for (int i = 0; i < num_blk; ++i) {
      auto trx_size = vec_trx_modes[i].size();
      s << trx_size;
      for (int j = 0; j < trx_size; ++j) {
         s << vec_trx_modes[i][j];
      }
    }
    return s.out();
  }
  // order of blocks (in hash)
  std::vector<vec_trx_t> blk_order;
  // It is multiple array of transactions
  std::vector<std::vector<uint8_t>> vec_trx_modes;
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
