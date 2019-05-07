/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-03-20 22:11:32
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-05-06
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

blk_hash_t PivotBlock::getPrevPivotBlockHash() const {
  return prev_pivot_blk_;
}

blk_hash_t PivotBlock::getPrevBlockHash() const {
  return prev_res_blk_;
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

ScheduleBlock::ScheduleBlock(taraxa::stream &strm) {
  deserialize(strm);
}

std::string ScheduleBlock::getJsonStr() const {
  std::stringstream strm;
  strm << "[ScheduleBlock] " << std::endl;
  strm << "block hash: " << block_hash_ << std::endl;
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
  return block_hash_;
}

bool ScheduleBlock::serialize(taraxa::stream& strm) const {
  bool ok = true;

  ok &= write(strm, block_hash_);
  ok &= write(strm, prev_pivot_);
  ok &= write(strm, timestamp_);
  ok &= write(strm, sig_);
  uint32_t block_size = schedule_.blk_order.size();
  uint32_t trx_vectors_size = schedule_.vec_trx_modes.size();
  if (block_size != trx_vectors_size) {
    std::cerr << "Number of Blocks should be equal to the size of transaction vectors"
              << std::endl;
    return false;
  }
  ok &= write(strm, block_size);
  ok &= write(strm, trx_vectors_size);
  for (int i = 0; i < block_size; i++) {
    ok &= write(strm, schedule_.blk_order[i]);
  }
  for (int i = 0; i < trx_vectors_size; i++) {
    uint32_t each_block_trx_size = schedule_.vec_trx_modes[i].size();
    ok &= write(strm, each_block_trx_size);
    for (int j = 0; j < each_block_trx_size; j++) {
      ok &= write(strm, schedule_.vec_trx_modes[i][j]);
    }
  }
  assert(ok);

  return ok;
}

bool ScheduleBlock::deserialize(taraxa::stream& strm) {
  bool ok = true;

  ok &= read(strm, block_hash_);
  ok &= read(strm, prev_pivot_);
  ok &= read(strm, timestamp_);
  ok &= read(strm, sig_);
  uint32_t block_size;
  uint32_t trx_vectors_size;
  ok &= read(strm, block_size);
  ok &= read(strm, trx_vectors_size);
  if (block_size != trx_vectors_size) {
    std::cerr << "Number of Blocks should be equal to the size of transaction vectors"
              << std::endl;
    return false;
  }
  for (int i = 0; i < block_size; i++) {
    blk_hash_t block_hash;
    ok &= read(strm, block_hash);
    if (ok) {
      schedule_.blk_order.push_back(block_hash);
    }
  }
  for (int i = 0; i < trx_vectors_size; i++) {
    uint32_t each_block_trx_size;
    ok &= read(strm, each_block_trx_size);
    std::vector<uint> each_block_trxs;
    for (int j = 0; j < each_block_trx_size; j++) {
      uint trx_mode;
      ok &= read(strm, trx_mode);
      if (ok) {
        each_block_trxs.push_back(trx_mode);
      }
    }
    schedule_.vec_trx_modes.push_back(each_block_trxs);
  }
  assert(ok);

  return ok;
}

PbftBlock::PbftBlock(dev::RLP const& _r) {
  std::vector<::byte> blockBytes;
  blockBytes = _r.toBytes();
  taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
  deserialize(strm);
}

blk_hash_t PbftBlock::getBlockHash() const {
  return block_hash_;
}

blk_hash_t PbftBlock::getPivotBlockHash() const {
  return pivot_block_.getHash();
}

blk_hash_t PbftBlock::getScheduleBlockHash() const {
  return schedule_block_.getHash();
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
  ok &= write(strm, block_hash_);
  ok &= write(strm, block_type_);
  if (block_type_ == pivot_block_type) {
    pivot_block_.serialize(strm);
  } else if (block_type_ == schedule_block_type) {
    schedule_block_.serialize(strm);
  }
  // TODO: serialize other pbft blocks
  assert(ok);
  return ok;
}

bool PbftBlock::deserialize(taraxa::stream& strm) {
  bool ok = true;
  ok &= read(strm, block_hash_);
  ok &= read(strm, block_type_);
  if (block_type_ == pivot_block_type) {
    pivot_block_.deserialize(strm);
  } else if (block_type_ == schedule_block_type) {
    schedule_block_.deserialize(strm);
  }
  // TODO: serialize other pbft blocks
  assert(ok);
  return ok;
}

blk_hash_t PbftChain::getLastPbftBlock() const {
  return last_pbft_blk_;
}

PbftBlockTypes PbftChain::getNextPbftBlockType() const {
  return next_pbft_block_type_;
}

size_t PbftChain::getPbftQueueSize() const {
  return pbft_queue_.size();
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

bool PbftChain::findPbftBlockInQueue(
    const taraxa::blk_hash_t& pbft_block_hash) {
  return pbft_queue_map_.find(pbft_block_hash) != pbft_queue_map_.end();
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
  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  insertPbftBlock(pbft_block_hash, pbft_block);
  setLastPbftBlock(pbft_block_hash);
  // TODO: only support pbft pivot and schedule block type so far
  // may need add pbft result block type later
  int next_pbft_block_type = (pbft_block.getBlockType() + 1) % 2;
  setNextPbftBlockType(static_cast<PbftBlockTypes>(next_pbft_block_type));
  count++;
}

bool PbftChain::pushPbftPivotBlock(taraxa::PbftBlock const& pbft_block) {
  if (pbft_block.getBlockType() != pivot_block_type) {
    return false;
  }
  if (next_pbft_block_type_ != pivot_block_type) {
    return false;
  }
  std::pair<PbftBlock, bool> pbft_chain_last_blk = getPbftBlock(last_pbft_blk_);
  if (!pbft_chain_last_blk.second) {
    std::cerr << "Cannot find the last pbft block in pbft chain" << std::endl;
    return false;
  }
  if (pbft_block.getPivotBlock().getPrevBlockHash() !=
      pbft_chain_last_blk.first.getBlockHash()) {
    return false;
  }
  pushPbftBlock(pbft_block);
  return true;
}

bool PbftChain::pushPbftScheduleBlock(taraxa::PbftBlock const& pbft_block) {
  if (pbft_block.getBlockType() != schedule_block_type) {
    return false;
  }
  if (next_pbft_block_type_ != schedule_block_type) {
    return false;
  }
  std::pair<PbftBlock, bool> pbft_chain_last_blk = getPbftBlock(last_pbft_blk_);
  if (!pbft_chain_last_blk.second) {
    std::cerr << "Cannot find the last pbft block in pbft chain" << std::endl;
    return false;
  }
  if (pbft_block.getScheduleBlock().getPrevBlockHash() !=
      pbft_chain_last_blk.first.getBlockHash()) {
    return false;
  }
  pushPbftBlock(pbft_block);
  return true;
}

void PbftChain::pushPbftBlockIntoQueue(taraxa::PbftBlock const& pbft_block) {
  pbft_queue_.emplace_back(pbft_block);
  pbft_queue_map_[pbft_block.getBlockHash()] = pbft_block;
}

}  // namespace taraxa
