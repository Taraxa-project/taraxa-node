#include "pbft_chain.hpp"
#include "pbft_manager.hpp"

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

PivotBlock::PivotBlock(taraxa::stream& strm) { deserialize(strm); }

blk_hash_t PivotBlock::getPrevPivotBlockHash() const {
  return prev_pivot_hash_;
}

blk_hash_t PivotBlock::getPrevBlockHash() const { return prev_block_hash_; }

blk_hash_t PivotBlock::getDagBlockHash() const { return dag_block_hash_; }

uint64_t PivotBlock::getPeriod() const { return period_; }

addr_t PivotBlock::getBeneficiary() const { return beneficiary_; }

void PivotBlock::setJsonTree(ptree& tree) const {
  tree.put("prev_pivot_hash", prev_pivot_hash_.toString());
  tree.put("prev_block_hash", prev_block_hash_.toString());
  tree.put("dag_block_hash", dag_block_hash_.toString());
  tree.put("period", period_);
  tree.put("beneficiary", beneficiary_.toString());
}

void PivotBlock::setBlockByJson(ptree const& doc) {
  prev_pivot_hash_ = blk_hash_t(doc.get<std::string>("prev_pivot_hash"));
  prev_block_hash_ = blk_hash_t(doc.get<std::string>("prev_block_hash"));
  dag_block_hash_ = blk_hash_t(doc.get<std::string>("dag_block_hash"));
  period_ = doc.get<uint64_t>("period");
  beneficiary_ = addr_t(doc.get<std::string>("beneficiary"));
}

bool PivotBlock::serialize(stream& strm) const {
  bool ok = true;

  ok &= write(strm, prev_pivot_hash_);
  ok &= write(strm, prev_block_hash_);
  ok &= write(strm, dag_block_hash_);
  ok &= write(strm, period_);
  ok &= write(strm, beneficiary_);
  assert(ok);

  return ok;
}

bool PivotBlock::deserialize(stream& strm) {
  bool ok = true;

  ok &= read(strm, prev_pivot_hash_);
  ok &= read(strm, prev_block_hash_);
  ok &= read(strm, dag_block_hash_);
  ok &= read(strm, period_);
  ok &= read(strm, beneficiary_);
  assert(ok);

  return ok;
}

void PivotBlock::streamRLP(dev::RLPStream& strm) const {
  strm << prev_pivot_hash_;
  strm << prev_block_hash_;
  strm << dag_block_hash_;
  strm << period_;
  strm << beneficiary_;
}

ScheduleBlock::ScheduleBlock(taraxa::stream& strm) { deserialize(strm); }

Json::Value ScheduleBlock::getJson() const {
  Json::Value res;
  res["prev_block_hash"] = dev::toJS(prev_block_hash_);
  Json::Value block_order = Json::Value(Json::arrayValue);
  for (auto const& b : this->schedule_.blk_order) {
    block_order.append(dev::toJS(b));
  }
  res["block_order"] = block_order;
  Json::Value trx_modes = Json::Value(Json::arrayValue);
  for (auto const& m1 : this->schedule_.vec_trx_modes) {
    Json::Value trx_modes_row = Json::Value(Json::arrayValue);
    for (auto const& m2 : m1) {
      trx_modes_row.append(dev::toJS(m2));
    }
    trx_modes.append(dev::toJS(trx_modes_row));
  }
  res["trx_modes"] = trx_modes;
  return res;
}

std::string ScheduleBlock::getJsonStr() const {
  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, getJson());
}

std::ostream& operator<<(std::ostream& strm, ScheduleBlock const& sche_blk) {
  strm << sche_blk.getJsonStr();
  return strm;
}

TrxSchedule ScheduleBlock::getSchedule() const { return schedule_; }

blk_hash_t ScheduleBlock::getPrevBlockHash() const { return prev_block_hash_; }

void ScheduleBlock::setJsonTree(ptree& tree) const {
  tree.put("prev_block_hash", prev_block_hash_.toString());

  tree.put_child("block_order", ptree());
  auto& block_order = tree.get_child("block_order");
  uint32_t block_size = schedule_.blk_order.size();
  for (int i = 0; i < block_size; i++) {
    block_order.push_back(
        std::make_pair("", ptree(schedule_.blk_order[i].toString())));
  }

  uint32_t trx_vectors_size = schedule_.vec_trx_modes.size();
  if (block_size != trx_vectors_size) {
    assert(false);
  }
  for (int i = 0; i < trx_vectors_size; i++) {
    blk_hash_t block_hash(schedule_.blk_order[i]);
    tree.put_child(block_hash.toString(), ptree());
    auto& trx_modes = tree.get_child(block_hash.toString());
    uint32_t each_block_trx_size = schedule_.vec_trx_modes[i].size();
    for (int j = 0; j < each_block_trx_size; j++) {
      trx_modes.push_back(std::make_pair(
          "", ptree(std::to_string(schedule_.vec_trx_modes[i][j]))));
    }
  }
}

void ScheduleBlock::setBlockByJson(ptree const& doc) {
  prev_block_hash_ = blk_hash_t(doc.get<std::string>("prev_block_hash"));
  schedule_.blk_order = asVector<blk_hash_t, std::string>(doc, "block_order");
  for (auto const& blk_hash : schedule_.blk_order) {
    std::vector<std::string> block_trx_modes_str =
        asVector<std::string, std::string>(doc, blk_hash.toString());
    std::vector<uint> block_trx_modes;
    for (auto const& mode : block_trx_modes_str) {
      block_trx_modes.emplace_back(atoi(mode.c_str()));
    }
    schedule_.vec_trx_modes.emplace_back(block_trx_modes);
  }
}

bool ScheduleBlock::serialize(taraxa::stream& strm) const {
  bool ok = true;

  ok &= write(strm, prev_block_hash_);
  uint32_t block_size = schedule_.blk_order.size();
  uint32_t trx_vectors_size = schedule_.vec_trx_modes.size();
  if (block_size != trx_vectors_size) {
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

  ok &= read(strm, prev_block_hash_);
  uint32_t block_size;
  uint32_t trx_vectors_size;
  ok &= read(strm, block_size);
  ok &= read(strm, trx_vectors_size);
  if (block_size != trx_vectors_size) {
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

void ScheduleBlock::streamRLP(dev::RLPStream& strm) const {
  strm << prev_block_hash_;
  for (int i = 0; i < schedule_.blk_order.size(); i++) {
    strm << schedule_.blk_order[i];
  }
  for (int i = 0; i < schedule_.vec_trx_modes.size(); i++) {
    for (int j = 0; j < schedule_.vec_trx_modes[i].size(); j++) {
      strm << schedule_.vec_trx_modes[i][j];
    }
  }
}
PbftBlock::PbftBlock(bytes const& b) : PbftBlock(dev::RLP(b)) {}

PbftBlock::PbftBlock(dev::RLP const& r) {
  auto blockbytes = r.toBytes();
  taraxa::bufferstream strm(blockbytes.data(), blockbytes.size());
  deserialize(strm);
}

PbftBlock::PbftBlock(std::string const& json) {
  ptree doc = strToJson(json);

  block_hash_ = blk_hash_t(doc.get<std::string>("block_hash"));
  block_type_ = static_cast<PbftBlockTypes>(doc.get<int>("block_type"));
  if (block_type_ == pivot_block_type) {
    ptree& pivot_block = doc.get_child("pivot_block");
    pivot_block_.setBlockByJson(pivot_block);
  } else if (block_type_ == schedule_block_type) {
    ptree& schedule_block = doc.get_child("schedule_block");
    schedule_block_.setBlockByJson(schedule_block);
  }  // TODO: more block types
  timestamp_ = doc.get<uint64_t>("timestamp");
  signature_ = sig_t(doc.get<std::string>("signature"));
}

void PbftBlock::setBlockHash() {
  dev::RLPStream s;
  streamRLP(s);
  block_hash_ = dev::sha3(s.out());
}

// Using to setup PBFT block hash
void PbftBlock::streamRLP(dev::RLPStream& strm) const {
  strm << block_type_;
  if (block_type_ == pivot_block_type) {
    pivot_block_.streamRLP(strm);
  } else if (block_type_ == schedule_block_type) {
    schedule_block_.streamRLP(strm);
  }  // TODO: more block types
  strm << timestamp_;
  strm << signature_;
}
bytes PbftBlock::rlp() const {
  RLPStream strm;
  serializeRLP(
      strm);  // CCL: to fix, this is not from real rlp, should use streamRLP
  return strm.out();
}
blk_hash_t PbftBlock::getBlockHash() const { return block_hash_; }

std::string PbftBlock::getJsonStr() const {
  ptree tree;
  tree.put("block_hash", block_hash_.toString());
  tree.put("block_type", block_type_);
  if (block_type_ == pivot_block_type) {
    tree.put_child("pivot_block", ptree());
    pivot_block_.setJsonTree(tree.get_child("pivot_block"));
  } else if (block_type_ == schedule_block_type) {
    tree.put_child("schedule_block", ptree());
    schedule_block_.setJsonTree(tree.get_child("schedule_block"));
  }  // TODO: more block types
  tree.put("timestamp", timestamp_);
  tree.put("signature", signature_.toString());

  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

PbftBlockTypes PbftBlock::getBlockType() const { return block_type_; }

void PbftBlock::setBlockType(taraxa::PbftBlockTypes block_type) {
  block_type_ = block_type;
}

PivotBlock PbftBlock::getPivotBlock() const { return pivot_block_; }

void PbftBlock::setPivotBlock(taraxa::PivotBlock const& pivot_block) {
  pivot_block_ = pivot_block;
  block_type_ = pivot_block_type;
}

ScheduleBlock PbftBlock::getScheduleBlock() const { return schedule_block_; }

void PbftBlock::setScheduleBlock(taraxa::ScheduleBlock const& schedule_block) {
  schedule_block_ = schedule_block;
  block_type_ = schedule_block_type;
}

uint64_t PbftBlock::getTimestamp() const { return timestamp_; }

void PbftBlock::setTimestamp(uint64_t const timestamp) {
  timestamp_ = timestamp;
}

void PbftBlock::setSignature(taraxa::sig_t const& signature) {
  signature_ = signature;
}

void PbftBlock::serializeRLP(dev::RLPStream& s) const {
  std::vector<uint8_t> bytes;
  {
    vectorstream strm(bytes);
    serialize(strm);
  }
  s.append(bytes);
}

bool PbftBlock::serialize(stream& strm) const {
  bool ok = true;
  ok &= write(strm, block_hash_);
  ok &= write(strm, block_type_);
  if (block_type_ == pivot_block_type) {
    pivot_block_.serialize(strm);
  } else if (block_type_ == schedule_block_type) {
    schedule_block_.serialize(strm);
  }
  ok &= write(strm, timestamp_);
  ok &= write(strm, signature_);
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
  ok &= read(strm, timestamp_);
  ok &= read(strm, signature_);
  // TODO: serialize other pbft blocks
  assert(ok);
  return ok;
}

std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk) {
  strm << pbft_blk.getJsonStr();
  return strm;
}

PbftChain::PbftChain(std::string const& dag_genesis_hash)
    : genesis_hash_(blk_hash_t(0)),
      size_(1),
      period_(0),
      next_pbft_block_type_(pivot_block_type),
      last_pbft_block_hash_(genesis_hash_),
      last_pbft_pivot_hash_(genesis_hash_),
      dag_genesis_hash_(blk_hash_t(dag_genesis_hash)) {}

void PbftChain::setFullNode(std::shared_ptr<taraxa::FullNode> full_node) {
  node_ = full_node;
  // setup pbftchain DB pointer
  db_pbftchain_ = full_node->getPbftChainDB();
  assert(db_pbftchain_);
  // setup PBFT blocks order DB pointers
  db_pbft_blocks_order_ = full_node->getPbftBlocksOrderDB();
  assert(db_pbft_blocks_order_);
  // setup DAG blocks order/height DB pointers
  db_dag_blocks_order_ = full_node->getDagBlocksOrderDB();
  assert(db_dag_blocks_order_);
  db_dag_blocks_height_ = full_node->getDagBlocksHeightDB();
  assert(db_dag_blocks_height_);

  // Get PBFT head from DB
  std::string pbft_genesis_str =
      db_pbftchain_->lookup(genesis_hash_.toString());
  if (pbft_genesis_str.empty()) {
    // Store PBFT chain genesis(HEAD) block to db
    insertPbftBlockIndex_(genesis_hash_);
    db_pbftchain_->insert(genesis_hash_.toString(), getJsonStr());
    // Initialize DAG genesis at DAG block heigh 1
    pushDagBlockHash(dag_genesis_hash_);
  } else {
    // set PBFT genesis from DB
    setPbftGenesis(pbft_genesis_str);
  }
}

void PbftChain::setPbftGenesis(std::string const& pbft_genesis_str) {
  ptree doc = strToJson(pbft_genesis_str);

  genesis_hash_ = blk_hash_t(doc.get<std::string>("genesis_hash"));
  size_ = doc.get<uint64_t>("size");
  period_ = doc.get<uint64_t>("period");
  next_pbft_block_type_ =
      static_cast<PbftBlockTypes>(doc.get<int>("next_pbft_block_type"));
  last_pbft_block_hash_ =
      blk_hash_t(doc.get<std::string>("last_pbft_block_hash"));
  last_pbft_pivot_hash_ =
      blk_hash_t(doc.get<std::string>("last_pbft_pivot_hash"));
}

void PbftChain::releaseDB() {
  db_pbftchain_ = nullptr;
  db_pbft_blocks_order_ = nullptr;
  db_dag_blocks_order_ = nullptr;
  db_dag_blocks_height_ = nullptr;
}

void PbftChain::cleanupUnverifiedPbftBlocks(
    taraxa::PbftBlock const& pbft_block) {
  blk_hash_t prev_block_hash;
  if (pbft_block.getBlockType() == pivot_block_type) {
    prev_block_hash = pbft_block.getPivotBlock().getPrevBlockHash();
  } else if (pbft_block.getBlockType() == schedule_block_type) {
    prev_block_hash = pbft_block.getScheduleBlock().getPrevBlockHash();
  } else {
    LOG(log_err_) << "Unknown PBFT block type " << pbft_block.getBlockType();
    assert(false);
  }
  upgradableLock_ lock(unverified_access_);
  if (unverified_blocks_map_.find(prev_block_hash) ==
      unverified_blocks_map_.end()) {
    LOG(log_err_) << "Cannot find the prev PBFT block hash " << prev_block_hash;
    assert(false);
  }
  // cleanup PBFT blocks in unverified_blocks_ table
  upgradeLock_ locked(lock);
  for (blk_hash_t const& block_hash : unverified_blocks_map_[prev_block_hash]) {
    unverified_blocks_.erase(block_hash);
  }
  // cleanup PBFT blocks hash in unverified_blocks_map_ table
  unverified_blocks_map_.erase(prev_block_hash);
}

uint64_t PbftChain::getPbftChainSize() const { return size_; }

uint64_t PbftChain::getPbftChainPeriod() const { return period_; }

blk_hash_t PbftChain::getGenesisHash() const { return genesis_hash_; }

blk_hash_t PbftChain::getLastPbftBlockHash() const {
  return last_pbft_block_hash_;
}

blk_hash_t PbftChain::getLastPbftPivotHash() const {
  return last_pbft_pivot_hash_;
}

PbftBlockTypes PbftChain::getNextPbftBlockType() const {
  return next_pbft_block_type_;
}

std::pair<blk_hash_t, bool> PbftChain::getDagBlockHash(
    uint64_t dag_block_height) const {
  if (dag_block_height > max_dag_blocks_height_) {
    LOG(log_err_) << "The DAG block height " << dag_block_height
                  << " is greater than current max dag blocks height "
                  << max_dag_blocks_height_;
    return std::make_pair(blk_hash_t(0), false);
  }
  std::string dag_block_hash_str =
      db_dag_blocks_order_->lookup(std::to_string(dag_block_height));
  if (dag_block_hash_str.empty()) {
    LOG(log_err_) << "The DAG block height " << dag_block_height
                  << " is not exist in DAG blocks order DB.";
    return std::make_pair(blk_hash_t(0), false);
  }
  return std::make_pair(blk_hash_t(dag_block_hash_str), true);
}

std::pair<uint64_t, bool> PbftChain::getDagBlockHeight(
    blk_hash_t const& dag_block_hash) const {
  std::string dag_block_height_str =
      db_dag_blocks_height_->lookup(dag_block_hash.toString());
  if (dag_block_height_str.empty()) {
    LOG(log_err_) << "Cannot find the DAG block hash " << dag_block_hash
                  << " in DAG blocks height DB";
    return std::make_pair(0, false);
  }
  uint64_t dag_block_height;
  std::istringstream iss(dag_block_height_str);
  iss >> dag_block_height;
  return std::make_pair(dag_block_height, true);
}

uint64_t PbftChain::getDagBlockMaxHeight() const {
  assert(max_dag_blocks_height_);
  return max_dag_blocks_height_;
}

void PbftChain::setLastPbftBlockHash(blk_hash_t const& new_pbft_block_hash) {
  last_pbft_block_hash_ = new_pbft_block_hash;
}

void PbftChain::setNextPbftBlockType(taraxa::PbftBlockTypes next_block_type) {
  next_pbft_block_type_ = next_block_type;
}

bool PbftChain::findPbftBlockInChain(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  if (!db_pbftchain_) {
    LOG(log_err_) << "Pbft chain DB unavailable in findPbftBlockInChain!";
    return false;
  }

  assert(db_pbftchain_);
  return db_pbftchain_->lookup(pbft_block_hash.toString()) != "";
}

bool PbftChain::findUnverifiedPbftBlock(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  sharedLock_ lock(unverified_access_);
  return unverified_blocks_.find(pbft_block_hash) != unverified_blocks_.end();
}

bool PbftChain::findPbftBlockInVerifiedSet(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  return pbft_verified_set_.find(pbft_block_hash) != pbft_verified_set_.end();
}

PbftBlock PbftChain::getPbftBlockInChain(
    const taraxa::blk_hash_t& pbft_block_hash) {
  std::string pbft_block_str =
      db_pbftchain_->lookup(pbft_block_hash.toString());
  if (pbft_block_str.empty()) {
    LOG(log_err_) << "Cannot find PBFT block hash " << pbft_block_hash
                  << " in DB";
    assert(false);
  }
  return PbftBlock(pbft_block_str);
}

std::pair<PbftBlock, bool> PbftChain::getUnverifiedPbftBlock(
    const taraxa::blk_hash_t& pbft_block_hash) {
  if (findUnverifiedPbftBlock(pbft_block_hash)) {
    sharedLock_ lock(unverified_access_);
    return std::make_pair(unverified_blocks_[pbft_block_hash], true);
  }
  return std::make_pair(PbftBlock(), false);
}

std::vector<PbftBlock> PbftChain::getPbftBlocks(size_t height,
                                                size_t count) const {
  std::vector<PbftBlock> result;
  for (auto i = height; i < height + count; i++) {
    std::string pbft_block_hash_str =
        db_pbft_blocks_order_->lookup(std::to_string(i));
    if (pbft_block_hash_str.empty()) {
      LOG(log_err_) << "PBFT block height " << i
                    << " is not exist in blocks order DB.";
      break;
    }
    std::string pbft_block_str = db_pbftchain_->lookup(pbft_block_hash_str);
    if (pbft_block_str.empty()) {
      LOG(log_err_) << "Cannot find PBFT block hash " << pbft_block_hash_str
                    << " in PBFT chain DB.";
      break;
    }
    result.push_back(PbftBlock(pbft_block_str));
  }
  return result;
}

bool PbftChain::pushPbftBlockIntoChain(taraxa::PbftBlock const& pbft_block) {
  if (db_pbftchain_->exists(pbft_block.getBlockHash().toString())) {
    LOG(log_err_) << "Failed put pbft block: " << pbft_block.getBlockHash()
                  << " into DB";
    return false;
  }
  db_pbftchain_->insert(pbft_block.getBlockHash().toString(),
                        pbft_block.getJsonStr());
  // To protect PBFT syncing from peers happening in the same time
  // PBFT chain DB commit first for block, and size_ increase later
  size_++;
  db_pbftchain_->insert(genesis_hash_.toString(), getJsonStr());

  return true;
}

bool PbftChain::pushPbftBlock(taraxa::PbftBlock const& pbft_block) {
  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  setLastPbftBlockHash(pbft_block_hash);
  pbftVerifiedSetInsert_(pbft_block.getBlockHash());
  // TODO: only support pbft pivot and schedule block type so far
  //  may need add pbft result block type later
  if (pbft_block.getBlockType() == pivot_block_type) {
    setNextPbftBlockType(schedule_block_type);
  } else if (pbft_block.getBlockType() == schedule_block_type) {
    setNextPbftBlockType(pivot_block_type);
  }
  if (!pushPbftBlockIntoChain(pbft_block)) {
    // TODO: need roll back
    assert(false);
    // return false;
  }
  insertPbftBlockIndex_(pbft_block_hash);
  if (pbft_block.getBlockType() == pivot_block_type) {
    // PBFT ancher block
    LOG(log_inf_) << "Push pbft block " << pbft_block_hash
                  << " with DAG block hash "
                  << pbft_block.getPivotBlock().getDagBlockHash()
                  << " into pbft chain, current pbft chain period " << period_
                  << " chain size is " << size_;
  } else if (pbft_block.getBlockType() == schedule_block_type) {
    // PBFT concurrent schedule block
    LOG(log_inf_) << "Push pbft block " << pbft_block_hash
                  << " into pbft chain, current pbft chain period " << period_
                  << " chain size is " << size_;
  }
  return true;
}

bool PbftChain::pushPbftPivotBlock(taraxa::PbftBlock const& pbft_block) {
  if (pbft_block.getBlockType() != pivot_block_type) {
    LOG(log_err_) << "pushPbftPivotBlock() found pbft block type not equal "
                     "pivot block type";
    return false;
  }
  if (next_pbft_block_type_ != pivot_block_type) {
    LOG(log_err_) << "pushPbftPivotBlock() found next_pbft_block_type_ not "
                     "equal pivot block type";
    return false;
  }
  if (!last_pbft_block_hash_) {
    // The first PBFT Pivot Block
    if (pbft_block.getPivotBlock().getPrevBlockHash() != genesis_hash_) {
      LOG(log_err_)
          << "pushPbftPivotBlock() found getPivotBlock().getPrevBlockHash() "
             "not equal genesis hash";
      return false;
    }
  } else {
    PbftBlock pbft_chain_last_blk = getPbftBlockInChain(last_pbft_block_hash_);
    if (pbft_block.getPivotBlock().getPrevBlockHash() !=
        pbft_chain_last_blk.getBlockHash()) {
      LOG(log_err_)
          << "pushPbftPivotBlock() found getPivotBlock().getPrevBlockHash() "
             "not equal pbft_chain_last_blk.getBlockHash()";
      return false;
    }
  }
  period_++;
  if (!pushPbftBlock(pbft_block)) {
    LOG(log_err_)
        << "pushPbftPivotBlock() found pushPbftBlock() returned false";
    return false;
  }
  last_pbft_pivot_hash_ = pbft_block.getBlockHash();
  return true;
}

bool PbftChain::pushPbftScheduleBlock(taraxa::PbftBlock const& pbft_block) {
  if (pbft_block.getBlockType() != schedule_block_type) {
    LOG(log_err_) << "pushPbftScheduleBlock() found pbft block type not equal "
                     "schedule block type";
    return false;
  }
  if (next_pbft_block_type_ != schedule_block_type) {
    LOG(log_err_) << "pushPbftScheduleBlock() found next_pbft_block_type_ not "
                     "equal schedule block type";
    return false;
  }
  PbftBlock pbft_chain_last_blk = getPbftBlockInChain(last_pbft_block_hash_);
  if (pbft_block.getScheduleBlock().getPrevBlockHash() !=
      pbft_chain_last_blk.getBlockHash()) {
    LOG(log_err_) << "pushPbftScheduleBlock() found "
                     "getScheduleBlock().getPrevBlockHash() not equal "
                     "pbft_chain_last_blk.getBlockHash()";
    return false;
  }
  if (!pushPbftBlock(pbft_block)) {
    LOG(log_err_)
        << "pushPbftScheduleBlock() found pushPbftBlock() returned false";
    return false;
  }
  return true;
}

void PbftChain::pushUnverifiedPbftBlock(taraxa::PbftBlock const& pbft_block) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable";
    assert(false);
  }

  blk_hash_t block_hash = pbft_block.getBlockHash();
  blk_hash_t prev_block_hash;
  if (pbft_block.getBlockType() == pivot_block_type) {
    prev_block_hash = pbft_block.getPivotBlock().getPrevBlockHash();
  } else if (pbft_block.getBlockType() == schedule_block_type) {
    prev_block_hash = pbft_block.getScheduleBlock().getPrevBlockHash();
  } else {
    LOG(log_err_) << "Unknown PBFT block type " << pbft_block.getBlockType();
    assert(false);
  }
  if (prev_block_hash != last_pbft_block_hash_) {
    if (findPbftBlockInChain(block_hash)) {
      // The block comes from slow node, drop
      return;
    } else {
      // TODO: The block comes from fast node that should insert.
      //  Or comes from malicious node, need check
    }
  }
  // Store in unverified_blocks_map_ for cleaning later
  insertUnverifiedPbftBlockIntoParentMap_(prev_block_hash, block_hash);
  // Store in unverified_blocks_ table
  LOG(log_deb_) << "Push unverified block " << block_hash
                << ". Pbft unverified blocks size: "
                << unverified_blocks_.size();
  uniqueLock_ lock(unverified_access_);
  unverified_blocks_[pbft_block.getBlockHash()] = pbft_block;
}

uint64_t PbftChain::pushDagBlockHash(const taraxa::blk_hash_t& dag_block_hash) {
  std::string dag_block_height_str =
      db_dag_blocks_height_->lookup(dag_block_hash.toString());
  if (!dag_block_height_str.empty()) {
    // The DAG block already exist
    LOG(log_inf_) << "Duplicate DAG block " << dag_block_hash
                  << " in DAG blocks height DB already";
    uint64_t dag_block_height;
    std::istringstream iss(dag_block_height_str);
    iss >> dag_block_height;
    return dag_block_height;
  }
  // push DAG block hash into DAG blocks order DB. DAG genesis at index 1
  max_dag_blocks_height_++;
  db_dag_blocks_order_->insert(std::to_string(max_dag_blocks_height_),
                               dag_block_hash.toString());
  // push DAG block hash into DAG blocks height DB
  // key : dag block hash, value : dag block height>
  // DAG genesis is block height 1
  db_dag_blocks_height_->insert(dag_block_hash.toString(),
                                std::to_string(max_dag_blocks_height_));
  return max_dag_blocks_height_;
}

std::string PbftChain::getGenesisStr() const {
  std::stringstream strm;
  strm << "[PbftChain]" << std::endl;
  strm << "genesis hash: " << genesis_hash_.toString() << std::endl;
  strm << "size: " << size_ << std::endl;
  strm << "period: " << period_ << std::endl;
  strm << "next pbft block type: " << next_pbft_block_type_ << std::endl;
  strm << "last pbft block hash: " << last_pbft_pivot_hash_.toString()
       << std::endl;
  strm << "last pbft pivot hash: " << last_pbft_pivot_hash_.toString()
       << std::endl;
  // TODO: need add more info
  return strm.str();
}

std::string PbftChain::getJsonStr() const {
  ptree tree;
  tree.put("genesis_hash", genesis_hash_.toString());
  tree.put("size", size_);
  tree.put("period", period_);
  tree.put("next_pbft_block_type", next_pbft_block_type_);
  tree.put("last_pbft_block_hash", last_pbft_block_hash_.toString());
  tree.put("last_pbft_pivot_hash", last_pbft_pivot_hash_.toString());
  // TODO: need add more info

  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain) {
  strm << pbft_chain.getGenesisStr();
  return strm;
}

size_t PbftChain::pbftVerifiedQueueSize() const {
  sharedLock_ lock(verified_access_);
  return pbft_verified_queue_.size();
}

bool PbftChain::pbftVerifiedQueueEmpty() const {
  sharedLock_ lock(verified_access_);
  return pbft_verified_queue_.empty();
}

PbftBlock PbftChain::pbftVerifiedQueueFront() const {
  sharedLock_ lock(verified_access_);
  return pbft_verified_queue_.front();
}

void PbftChain::pbftVerifiedQueuePopFront() {
  pbftVerifiedSetErase_();
  uniqueLock_ lock(verified_access_);
  pbft_verified_queue_.pop_front();
}

void PbftChain::setVerifiedPbftBlockIntoQueue(PbftBlock const& pbft_block) {
  LOG(log_inf_) << "get pbft block " << pbft_block.getBlockHash()
                << " from peer and push into verified queue";
  uniqueLock_ lock(verified_access_);
  pbft_verified_queue_.emplace_back(pbft_block);
  pbft_verified_set_.insert(pbft_block.getBlockHash());
}

void PbftChain::pbftVerifiedSetInsert_(blk_hash_t const& pbft_block_hash) {
  uniqueLock_ lock(verified_access_);
  pbft_verified_set_.insert(pbft_block_hash);
}

void PbftChain::pbftVerifiedSetErase_() {
  PbftBlock pbft_block = pbftVerifiedQueueFront();
  uniqueLock_ lock(verified_access_);
  pbft_verified_set_.erase(pbft_block.getBlockHash());
}

void PbftChain::insertPbftBlockIndex_(
    taraxa::blk_hash_t const& pbft_block_hash) {
  db_pbft_blocks_order_->insert(std::to_string(size_),
                                pbft_block_hash.toString());
}

void PbftChain::insertUnverifiedPbftBlockIntoParentMap_(
    blk_hash_t const& prev_block_hash, blk_hash_t const& block_hash) {
  upgradableLock_ lock(unverified_access_);
  if (unverified_blocks_map_.find(prev_block_hash) ==
      unverified_blocks_map_.end()) {
    upgradeLock_ locked(lock);
    unverified_blocks_map_[prev_block_hash] = {block_hash};
  } else {
    upgradeLock_ locked(lock);
    unverified_blocks_map_[prev_block_hash].emplace_back(block_hash);
  }
}

PbftBlockCert::PbftBlockCert(PbftBlock const& pbft_blk,
                             std::vector<Vote> const& cert_votes)
    : pbft_blk(pbft_blk), cert_votes(cert_votes) {}

PbftBlockCert::PbftBlockCert(bytes const& all_rlp) {
  dev::RLP const rlp(all_rlp);
  auto num_items = rlp.itemCount();
  pbft_blk = PbftBlock(rlp[0].toBytes());
  PbftBlock t(rlp[0]);
  for (auto i = 1; i < num_items; ++i) {
    cert_votes.emplace_back(Vote(rlp[i].toBytes()));
  }
}

PbftBlockCert::PbftBlockCert(PbftBlock const& pbft_blk,
                             bytes const& cert_votes_rlp)
    : pbft_blk(pbft_blk) {
  auto rlp = dev::RLP(cert_votes_rlp);
  auto num_votes = rlp.itemCount();
  for (auto i = 0; i < num_votes; ++i) {
    cert_votes.emplace_back(rlp[i].toBytes());
  }
}

bytes PbftBlockCert::rlp() const {
  RLPStream s;
  s.appendList(cert_votes.size() + 1);
  s.append(pbft_blk.rlp());
  for (auto const& v : cert_votes) {
    s.append(v.rlp());
  }
  return s.out();
}

std::ostream& operator<<(std::ostream& strm, PbftBlockCert const& b) {
  strm << "[PbftBlockCert] hash: " << b.pbft_blk.getBlockHash()
       << " , num of votes " << b.cert_votes.size() << std::endl;
  return strm;
}

}  // namespace taraxa
