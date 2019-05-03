/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:32
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-01
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

PivotBlock::PivotBlock(taraxa::stream &strm) {
  deserialize(strm);
}

blk_hash_t PivotBlock::getHash() const {
  return block_hash_;
}

bool PivotBlock::serialize(stream &strm) const {
  bool ok = true;

  ok &= write(strm, block_hash_);
  ok &= write(strm, prev_pivot_blk_);
  ok &= write(strm, prev_res_blk_);
  ok &= write(strm, dag_blk_);
  ok &= write(strm, epoch_);
  ok &= write(strm, timestamp_);
  ok &= write(strm, beneficiary_);
  ok &= write(strm, sig_);
  assert(ok);

  return ok;
}

bool PivotBlock::deserialize(stream &strm) {
  bool ok = true;

  ok &= read(strm, block_hash_);
  ok &= read(strm, prev_pivot_blk_);
  ok &= read(strm, prev_res_blk_);
  ok &= read(strm, dag_blk_);
  ok &= read(strm, epoch_);
  ok &= read(strm, timestamp_);
  ok &= read(strm, beneficiary_);
  ok &= read(strm, sig_);
  assert(ok);

  return ok;
}
/*
TODO:: need fix later
ScheduleBlock::ScheduleBlock(dev::RLP const &rlp) {
  deserialize(strm);
}
*/
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

blk_hash_t ScheduleBlock::getHash() const {
  return schedule_blk_;
}

bool ScheduleBlock::serialize(taraxa::stream& strm) const {
  bool ok = true;

  ok &= write(strm, schedule_blk_);
  ok &= write(strm, prev_pivot_);
  ok &= write(strm, timestamp_);
  ok &= write(strm, sig_);
  ok &= write(strm, schedule_);
  assert(ok);

  return ok;
}

bool ScheduleBlock::deserialize(taraxa::stream& strm) {
  bool ok = true;

  ok &= read(strm, schedule_blk_);
  ok &= read(strm, prev_pivot_);
  ok &= read(strm, timestamp_);
  ok &= read(strm, sig_);
  ok &= read(strm, schedule_);
  assert(ok);

  return ok;
}

PbftBlock::PbftBlock(dev::RLP const& _r) {
  std::vector<::byte> blockBytes;
  blockBytes = _r.toBytes();
  taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
  deserialize(strm);
}

blk_hash_t PbftBlock::getHash() const {
  if (block_type_ == pivot_block_type) {
    return pivot_block_.getHash();
  } else {
    // TODO: change later, need add others pbft block type
    return pivot_block_.getHash();
  }
}

void PbftBlock::serializeRLP(dev::RLPStream& s) const {
  std::vector<uint8_t> bytes;
  {
    vectorstream strm(bytes);
    serialize(strm);
  }
  s.append(bytes);
}

bool PbftBlock::serialize(stream &strm) const {
  bool ok = true;
  ok &= write(strm, block_type_);
  if (block_type_ == pivot_block_type) {
    pivot_block_.serialize(strm);
  }
  // TODO: serialize other pbft blocks
  assert(ok);
  return ok;
}

bool PbftBlock::deserialize(taraxa::stream& strm) {
  bool ok = true;
  ok &= read(strm, block_type_);
  if (block_type_ == pivot_block_type) {
    pivot_block_.deserialize(strm);
  }
  // TODO: serialize other pbft blocks
  assert(ok);
  return ok;
}

blk_hash_t PbftChain::getLastPbftBlock() const {
  return last_pbft_blk_;
}

bool PbftChain::isPbftGenesis() const {
  return is_pbft_genesis_;
}

void PbftChain::setNotPbftGenesis() {
  is_pbft_genesis_ = false;
}

void PbftChain::setLastPbftBlock(const blk_hash_t &new_pbft_block) {
  last_pbft_blk_ = new_pbft_block;
}

void PbftChain::setNextPbftBlockType(taraxa::PbftBlockTypes next_block_type) {
  next_pbft_block_type_ = next_block_type;
}

bool PbftChain::findPbftBlock(const taraxa::blk_hash_t &pbft_block_hash) {
  return pbft_blocks_map_.find(pbft_block_hash) != pbft_blocks_map_.end();
}

std::pair<PbftBlock, bool> PbftChain::getPbftBlock(
    const taraxa::blk_hash_t &pbft_block_hash) {
  if (findPbftBlock(pbft_block_hash)) {
    return std::make_pair(pbft_blocks_map_[pbft_block_hash], true);
  }
  return std::make_pair(PbftBlock(), false);
}

void PbftChain::insertPbftBlock(const taraxa::blk_hash_t &pbft_block_hash,
                                const taraxa::PbftBlock &pbft_block) {
  pbft_blocks_map_[pbft_block_hash] = pbft_block;
}

void PbftChain::pushPbftBlock(const taraxa::PbftBlock &pbft_block) {
  blk_hash_t pbft_block_hash = pbft_block.getHash();
  insertPbftBlock(pbft_block_hash, pbft_block);
  setLastPbftBlock(pbft_block_hash);
  int next_pbft_block_type = (pbft_block.getBlockType() + 1) % 3;
  setNextPbftBlockType(static_cast<PbftBlockTypes>(next_pbft_block_type));
  count++;
}

}  // namespace taraxa
