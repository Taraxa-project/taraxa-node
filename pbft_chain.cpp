/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:32
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-08 18:56:46
 */

#include "pbft_chain.hpp"

namespace taraxa {
std::string TrxSchedule::getJsonStr() const {
  std::stringstream strm;
  strm << "  |TrxSchedule| " << std::endl;
  for (auto i = 0; i < blk_order.size(); i++) {
    strm << "  B: " << i << "  " << blk_order[i] << std::endl;
  }
  for (auto i = 0; i < vec_trx_modes.size(); i++) {
    strm << "  B: " << i << " , T: ";
    for (auto j = 0; j < vec_trx_modes[i].size(); j++) {
      strm << vec_trx_modes[i][j] << " ";
    }
    strm << std::endl;
  }
  return strm.str();
}
std::ostream& operator<<(std::ostream& strm, TrxSchedule const& tr_sche) {
  strm << tr_sche.getJsonStr();
  return strm;
}

TrxSchedule::TrxSchedule(bytes const& rlpData) {
  if (rlpData.empty()) return;
  dev::RLP const rlp(rlpData);
  try {
    if (!rlp.isList()) {
      assert(0);
    }
    size_t count = 0;
    auto num_blks = rlp[count++].toInt<size_t>();
    assert(num_blks >= 0);
    blk_order.resize(num_blks);
    vec_trx_modes.resize(num_blks);
    // reconstruct blk order
    for (auto i = 0; i < num_blks; ++i) {
      blk_order[i] = rlp[count++].toHash<blk_hash_t>();
    }
    // reconstruct trx
    for (auto i = 0; i < num_blks; ++i) {
      auto num_trx = rlp[count++].toInt<size_t>();
      for (auto j = 0; j < num_trx; ++j) {
        vec_trx_modes[i].push_back(rlp[count++].toInt<size_t>());
      }
    }
  } catch (std::exception& e) {
    std::cerr << "RLP decode error: " << e.what() << std::endl;
  }
}

bytes TrxSchedule::rlp() const {
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

std::string ScheduleBlock::getJsonStr() const {
  std::stringstream strm;
  strm << "[ScheduleBlock] " << std::endl;
  strm << "prev_pivot: " << prev_pivot_ << std::endl;
  strm << "time_stamp: " << timestamp_ << std::endl;
  strm << "sig: " << sig_ << std::endl;
  strm << "  --> Schedule ..." << std::endl;
  strm << schedule_;
  return strm.str();
}

std::ostream& operator<<(std::ostream& strm, ScheduleBlock const& sche_blk) {
  strm << sche_blk.getJsonStr();
  return strm;
}
}  // namespace taraxa
