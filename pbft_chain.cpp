#include "pbft_chain.hpp"

#include "pbft_manager.hpp"

namespace taraxa {
TrxSchedule::TrxSchedule(bytes const& rlpData) {
  if (rlpData.empty()) {
    return;
  }
  dev::RLP const rlp(rlpData);
  if (!rlp.isList()) {
    assert(false);
  }
  size_t count = 0;
  auto num_blks = rlp[count++].toInt<size_t>();
  assert(num_blks >= 0);
  dag_blks_order.resize(num_blks);
  trxs_mode.resize(num_blks);
  // reconstruct blk order
  for (auto i(0); i < num_blks; ++i) {
    dag_blks_order[i] = rlp[count++].toHash<blk_hash_t>();
  }
  // reconstruct trx
  for (auto i(0); i < num_blks; ++i) {
    auto num_trx = rlp[count++].toInt<size_t>();
    for (auto j(0); j < num_trx; ++j) {
      trx_hash_t trx = rlp[count++].toHash<trx_hash_t>();
      uint mode = rlp[count++].toInt<size_t>();
      trxs_mode[i].emplace_back(std::make_pair(trx, mode));
    }
  }
}

bytes TrxSchedule::rlp() const {
  dev::RLPStream s;
  auto num_blk = dag_blks_order.size();
  std::vector<size_t> trx_in_each_blk;
  size_t num_trx = 0;
  for (auto i(0); i < trxs_mode.size(); ++i) {
    trx_in_each_blk.emplace_back(trxs_mode[i].size());
    num_trx += trxs_mode[i].size();
  }
  s.appendList(1 + num_blk + num_blk + num_trx + num_trx);
  // write number of DAG blks
  s << num_blk;
  // write DAG blk hashes
  for (auto i(0); i < num_blk; ++i) {
    s << dag_blks_order[i];
  }
  // write trx info
  for (int i(0); i < num_blk; ++i) {
    auto trx_size = trxs_mode[i].size();
    s << trx_size;
    for (int j(0); j < trx_size; ++j) {
      s << trxs_mode[i][j].first;
      s << trxs_mode[i][j].second;
    }
  }
  return s.out();
}

bool TrxSchedule::serialize(taraxa::stream& strm) const {
  bool ok = true;
  uint32_t dag_blk_size = dag_blks_order.size();
  uint32_t trx_vectors_size = trxs_mode.size();
  if (dag_blk_size != trx_vectors_size) {
    assert(false);
  }
  ok &= write(strm, dag_blk_size);
  ok &= write(strm, trx_vectors_size);
  for (auto i(0); i < dag_blk_size; ++i) {
    ok &= write(strm, dag_blks_order[i]);
  }
  for (auto i(0); i < trx_vectors_size; ++i) {
    uint32_t each_block_trx_size = trxs_mode[i].size();
    ok &= write(strm, each_block_trx_size * 2);
    for (auto j(0); j < each_block_trx_size; ++j) {
      ok &= write(strm, trxs_mode[i][j].first);   // Transation hash
      ok &= write(strm, trxs_mode[i][j].second);  // Transation mode
    }
  }
  assert(ok);
  return ok;
}

bool TrxSchedule::deserialize(taraxa::stream& strm) {
  bool ok = true;
  uint32_t block_size;
  uint32_t trx_vectors_size;
  ok &= read(strm, block_size);
  ok &= read(strm, trx_vectors_size);
  if (block_size != trx_vectors_size) {
    assert(false);
  }
  for (auto i(0); i < block_size; ++i) {
    blk_hash_t dag_blk_hash;
    ok &= read(strm, dag_blk_hash);
    if (ok) {
      dag_blks_order.push_back(dag_blk_hash);
    }
  }
  for (auto i(0); i < trx_vectors_size; ++i) {
    uint32_t each_block_trx_size;
    ok &= read(strm, each_block_trx_size);
    std::vector<std::pair<trx_hash_t, uint>> each_block_trxs_mode;
    for (auto j(0); j < each_block_trx_size; ++j) {
      trx_hash_t trx;
      ok &= read(strm, trx);
      if (!ok) {
        assert(false);
      }
      j++;
      uint mode;
      ok &= read(strm, mode);
      if (!ok) {
        assert(false);
      }
      each_block_trxs_mode.emplace_back(std::make_pair(trx, mode));
    }
    trxs_mode.emplace_back(each_block_trxs_mode);
  }
  assert(ok);
  return ok;
}

void TrxSchedule::setPtree(ptree& tree) const {
  tree.put_child("dag_blocks_order", ptree());
  auto& dag_blocks_order = tree.get_child("dag_blocks_order");
  uint32_t dag_blocks_size = dag_blks_order.size();
  for (auto i(0); i < dag_blocks_size; ++i) {
    dag_blocks_order.push_back(
        std::make_pair("", ptree(dag_blks_order[i].toString())));
  }
  uint32_t trx_vectors_size = trxs_mode.size();
  if (dag_blocks_size != trx_vectors_size) {
    assert(false);
  }
  for (auto i(0); i < trx_vectors_size; ++i) {
    blk_hash_t dag_block_hash(dag_blks_order[i]);
    tree.put_child(dag_block_hash.toString(), ptree());
    auto& each_dag_block_trxs_mode = tree.get_child(dag_block_hash.toString());
    uint32_t each_dag_block_trxs_size = trxs_mode[i].size();
    for (int j(0); j < each_dag_block_trxs_size; ++j) {
      each_dag_block_trxs_mode.push_back(
          std::make_pair(trxs_mode[i][j].first.toString(),
                         ptree(std::to_string(trxs_mode[i][j].second))));
    }
  }
}

void TrxSchedule::setSchedule(ptree const& tree) {
  dag_blks_order = asVector<blk_hash_t, std::string>(tree, "dag_blocks_order");
  for (auto const& dag_blk_hash : dag_blks_order) {
    std::vector<std::pair<trx_hash_t, uint>> each_dag_blk_trxs_mode;
    for (auto& trx_mode : tree.get_child(dag_blk_hash.toString())) {
      trx_hash_t trx(trx_mode.first);
      uint mode = atoi(trx_mode.second.get_value<std::string>().c_str());
      each_dag_blk_trxs_mode.emplace_back(std::make_pair(trx, mode));
    }
    trxs_mode.emplace_back(each_dag_blk_trxs_mode);
  }
}

std::string TrxSchedule::getStr() const {
  std::stringstream strm;
  strm << "  |TrxSchedule| " << std::endl;
  for (auto i = 0; i < dag_blks_order.size(); i++) {
    strm << "  B: " << i << "  " << dag_blks_order[i] << std::endl;
  }
  for (auto i = 0; i < trxs_mode.size(); i++) {
    strm << "  B: " << i << " , T: ";
    for (auto j = 0; j < trxs_mode[i].size(); j++) {
      strm << trxs_mode[i][j].first << ", mode: " << trxs_mode[i][j].second;
    }
    strm << std::endl;
  }
  return strm.str();
}

std::ostream& operator<<(std::ostream& strm, TrxSchedule const& trx_sche) {
  strm << trx_sche.getStr();
  return strm;
}

PbftBlock::PbftBlock(bytes const& b) : PbftBlock(dev::RLP(b)) {}

PbftBlock::PbftBlock(dev::RLP const& r) {
  auto blockbytes = r.toBytes();
  taraxa::bufferstream strm(blockbytes.data(), blockbytes.size());
  deserialize(strm);
}

PbftBlock::PbftBlock(std::string const& str) {
  ptree doc = strToJson(str);
  block_hash_ = blk_hash_t(doc.get<std::string>("block_hash"));
  prev_block_hash_ = blk_hash_t(doc.get<std::string>("prev_block_hash"));
  dag_block_hash_as_pivot_ =
      blk_hash_t(doc.get<std::string>("dag_block_hash_as_pivot"));
  ptree& schedule = doc.get_child("schedule");
  schedule_.setSchedule(schedule);
  period_ = doc.get<uint64_t>("period");
  height_ = doc.get<uint64_t>("height");
  timestamp_ = doc.get<uint64_t>("timestamp");
  beneficiary_ = addr_t(doc.get<std::string>("beneficiary"));
  signature_ = sig_t(doc.get<std::string>("signature"));
}

bool PbftBlock::serialize(stream& strm) const {
  bool ok = true;
  ok &= write(strm, block_hash_);
  ok &= write(strm, prev_block_hash_);
  ok &= write(strm, dag_block_hash_as_pivot_);
  schedule_.serialize(strm);
  ok &= write(strm, period_);
  ok &= write(strm, height_);
  ok &= write(strm, timestamp_);
  ok &= write(strm, beneficiary_);
  ok &= write(strm, signature_);
  assert(ok);
  return ok;
}

bool PbftBlock::deserialize(taraxa::stream& strm) {
  bool ok = true;
  ok &= read(strm, block_hash_);
  ok &= read(strm, prev_block_hash_);
  ok &= read(strm, dag_block_hash_as_pivot_);
  schedule_.deserialize(strm);
  ok &= read(strm, period_);
  ok &= read(strm, height_);
  ok &= read(strm, timestamp_);
  ok &= read(strm, beneficiary_);
  ok &= read(strm, signature_);
  assert(ok);
  return ok;
}

std::string PbftBlock::getJsonStr(bool with_signature) const {
  ptree tree;
  tree.put("prev_block_hash", prev_block_hash_.toString());
  tree.put("dag_block_hash_as_pivot", dag_block_hash_as_pivot_.toString());
  tree.put_child("schedule", ptree());
  schedule_.setPtree(tree.get_child("schedule"));
  tree.put("period", period_);
  tree.put("height", height_);
  tree.put("timestamp", timestamp_);
  tree.put("beneficiary", beneficiary_);
  if (with_signature) {
    tree.put("block_hash", block_hash_.toString());
    tree.put("signature", signature_.toString());
  }
  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

addr_t PbftBlock::getAuthor() const {
  assert(!signature_.isZero());
  return dev::toAddress(dev::recover(signature_, dev::sha3(getJsonStr(false))));
}

// Using to setup PBFT block hash
void PbftBlock::streamRLP(dev::RLPStream& strm) const {
  strm << prev_block_hash_;
  strm << dag_block_hash_as_pivot_;
  // Schedule RLPStream
  for (auto const& dag_blk_hash : schedule_.dag_blks_order) {
    strm << dag_blk_hash;
  }
  for (auto const& each_dag_blk_trxs_mode : schedule_.trxs_mode) {
    for (auto const& trx_mode : each_dag_blk_trxs_mode) {
      strm << trx_mode.first;
      strm << trx_mode.second;
    }
  }
  strm << period_;
  strm << height_;
  strm << timestamp_;
  strm << beneficiary_;
  strm << signature_;
}

bytes PbftBlock::rlp() const {
  RLPStream strm;
  serializeRLP(
      strm);  // CCL: to fix, this is not from real rlp, should use streamRLP
  return strm.out();
}

void PbftBlock::serializeRLP(dev::RLPStream& s) const {
  std::vector<uint8_t> bytes;
  {
    vectorstream strm(bytes);
    serialize(strm);
  }
  s.append(bytes);
}

void PbftBlock::setBlockHash() {
  dev::RLPStream s;
  streamRLP(s);
  block_hash_ = dev::sha3(s.out());
}

void PbftBlock::setSignature(taraxa::sig_t const& signature) {
  signature_ = signature;
}

std::ostream& operator<<(std::ostream& strm, PbftBlock const& pbft_blk) {
  strm << pbft_blk.getJsonStr();
  return strm;
}

PbftBlockCert::PbftBlockCert(PbftBlock const& pbft_blk,
                             std::vector<Vote> const& cert_votes)
    : pbft_blk(pbft_blk), cert_votes(cert_votes) {}

PbftBlockCert::PbftBlockCert(bytes const& all_rlp) {
  dev::RLP const rlp(all_rlp);
  auto num_items = rlp.itemCount();
  pbft_blk = PbftBlock(rlp[0].toBytes());
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
  strm << "[PbftBlockCert] : " << b.pbft_blk << " , num of votes "
       << b.cert_votes.size() << std::endl;
  return strm;
}

PbftChain::PbftChain(std::string const& dag_genesis_hash)
    : genesis_hash_(blk_hash_t(0)),
      size_(1),
      period_(0),
      last_pbft_block_hash_(genesis_hash_),
      dag_genesis_hash_(blk_hash_t(dag_genesis_hash)) {}

void PbftChain::setFullNode(std::shared_ptr<taraxa::FullNode> full_node) {
  node_ = full_node;
  // setup pbftchain DB pointer
  db_ = full_node->getDB();
  assert(db_);

  // Get PBFT head from DB
  auto pbft_genesis_str = db_->getPbftBlockGenesis(genesis_hash_.toString());
  if (pbft_genesis_str.empty()) {
    // Store PBFT chain genesis(HEAD) block to db
    insertPbftBlockIndex_(genesis_hash_);
    db_->savePbftBlockGenesis(genesis_hash_.toString(), getJsonStr());
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
  last_pbft_block_hash_ =
      blk_hash_t(doc.get<std::string>("last_pbft_block_hash"));
}

void PbftChain::cleanupUnverifiedPbftBlocks(
    taraxa::PbftBlock const& pbft_block) {
  blk_hash_t prev_block_hash = pbft_block.getPrevBlockHash();
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

std::pair<blk_hash_t, bool> PbftChain::getDagBlockHash(
    uint64_t dag_block_height) const {
  if (dag_block_height > max_dag_blocks_height_) {
    LOG(log_err_) << "The DAG block height " << dag_block_height
                  << " is greater than current max dag blocks height "
                  << max_dag_blocks_height_;
    return std::make_pair(blk_hash_t(0), false);
  }
  auto dag_block_hash = db_->getDagBlockOrder(std::to_string(dag_block_height));
  if (dag_block_hash == nullptr) {
    LOG(log_err_) << "The DAG block height " << dag_block_height
                  << " is not exist in DAG blocks order DB.";
    return std::make_pair(blk_hash_t(0), false);
  }
  return std::make_pair(*dag_block_hash, true);
}

std::pair<uint64_t, bool> PbftChain::getDagBlockHeight(
    blk_hash_t const& dag_block_hash) const {
  std::string dag_block_height_str = db_->getDagBlockHeight(dag_block_hash);
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
  return max_dag_blocks_height_;
}

void PbftChain::setLastPbftBlockHash(blk_hash_t const& new_pbft_block_hash) {
  last_pbft_block_hash_ = new_pbft_block_hash;
}

bool PbftChain::findPbftBlockInChain(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  if (!db_) {
    LOG(log_err_) << "Pbft chain DB unavailable in findPbftBlockInChain!";
    return false;
  }

  assert(db_);
  return db_->pbftBlockInDb(pbft_block_hash);
}

bool PbftChain::findUnverifiedPbftBlock(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  sharedLock_ lock(unverified_access_);
  return unverified_blocks_.find(pbft_block_hash) != unverified_blocks_.end();
}

bool PbftChain::findPbftBlockInSyncedSet(
    taraxa::blk_hash_t const& pbft_block_hash) const {
  return pbft_synced_set_.find(pbft_block_hash) != pbft_synced_set_.end();
}

PbftBlock PbftChain::getPbftBlockInChain(
    const taraxa::blk_hash_t& pbft_block_hash) {
  auto pbft_block = db_->getPbftBlock(pbft_block_hash);
  if (pbft_block == nullptr) {
    LOG(log_err_) << "Cannot find PBFT block hash " << pbft_block_hash
                  << " in DB";
    assert(false);
  }
  return *pbft_block;
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
    auto pbft_block_hash = db_->getPbftBlockOrder(std::to_string(i));
    if (pbft_block_hash == nullptr) {
      LOG(log_err_) << "PBFT block height " << i
                    << " is not exist in blocks order DB.";
      break;
    }
    auto pbft_block = db_->getPbftBlock(*pbft_block_hash);
    if (pbft_block == nullptr) {
      LOG(log_err_) << "Cannot find PBFT block hash " << pbft_block_hash
                    << " in PBFT chain DB.";
      break;
    }
    if (pbft_block->getHeight() != i) {
      LOG(log_sil_) << "DB corrupted - PBFT block hash " << pbft_block_hash
                    << "has different height " << pbft_block->getHeight()
                    << "in block data then in block order db: " << i;
      assert(false);
    }
    result.push_back(*pbft_block);
  }
  return result;
}

std::vector<std::string> PbftChain::getPbftBlocksStr(size_t height,
                                                     size_t count,
                                                     bool hash) const {
  std::vector<std::string> result;
  for (auto i = height; i < height + count; i++) {
    auto pbft_block_hash = db_->getPbftBlockOrder(std::to_string(i));
    if (pbft_block_hash == nullptr) {
      LOG(log_err_) << "PBFT block height " << i
                    << " is not exist in blocks order DB.";
      break;
    }
    auto pbft_block = db_->getPbftBlock(*pbft_block_hash);
    if (pbft_block == nullptr) {
      LOG(log_err_) << "Cannot find PBFT block hash "
                    << pbft_block_hash->toString() << " in PBFT chain DB.";
      break;
    }
    if (hash)
      result.push_back(pbft_block_hash->toString());
    else
      result.push_back(pbft_block->getJsonStr());
  }
  return result;
}

bool PbftChain::pushPbftBlockIntoChain(taraxa::PbftBlock const& pbft_block) {
  if (db_->pbftBlockInDb(pbft_block.getBlockHash())) {
    LOG(log_err_) << "Failed put pbft block: " << pbft_block.getBlockHash()
                  << " into DB";
    return false;
  }
  db_->savePbftBlock(pbft_block);
  // To protect PBFT syncing from peers happening in the same time
  // PBFT chain DB commit first for block, and size_ increase later
  size_++;
  db_->savePbftBlockGenesis(genesis_hash_.toString(), getJsonStr());
  return true;
}

bool PbftChain::pushPbftBlock(taraxa::PbftBlock const& pbft_block) {
  if (!checkPbftBlockValidation(pbft_block)) {
    return false;
  }
  blk_hash_t pbft_block_hash = pbft_block.getBlockHash();
  setLastPbftBlockHash(pbft_block_hash);
  pbftSyncedSetInsert_(pbft_block.getBlockHash());
  period_++;
  if (!pushPbftBlockIntoChain(pbft_block)) {
    // TODO: need roll back
    // Roll back PBFT period
    period_--;
    assert(false);
    // return false;
  }
  assert(pbft_block.getHeight() == size_);
  insertPbftBlockIndex_(pbft_block_hash);
  LOG(log_inf_) << "Push pbft block " << pbft_block_hash
                << " with DAG block hash " << pbft_block.getPivotDagBlockHash()
                << " into pbft chain, current pbft chain period " << period_
                << " chain size is " << size_;
  return true;
}

bool PbftChain::checkPbftBlockValidationFromSyncing(
    taraxa::PbftBlock const& pbft_block) const {
  if (pbftSyncedQueueEmpty()) {
    // The last PBFT block in the chain
    return checkPbftBlockValidation(pbft_block);
  } else {
    // The last PBFT block in the synced queue
    PbftBlock last_pbft_block = pbftSyncedQueueBack().pbft_blk;
    if (pbft_block.getPrevBlockHash() != last_pbft_block.getBlockHash()) {
      LOG(log_err_) << "Last PBFT block hash " << last_pbft_block.getBlockHash()
                    << " Invalid PBFT prev block hash "
                    << pbft_block.getPrevBlockHash();
      return false;
    }
    // TODO: Need to verify block signature
    return true;
  }
}

bool PbftChain::checkPbftBlockValidation(
    taraxa::PbftBlock const& pbft_block) const {
  if (pbft_block.getPrevBlockHash() != last_pbft_block_hash_) {
    LOG(log_err_) << "PBFT chain last block hash " << last_pbft_block_hash_
                  << " Invalid PBFT prev block hash "
                  << pbft_block.getPrevBlockHash();
    return false;
  }
  // TODO: Need to verify block signature
  return true;
}

void PbftChain::pushUnverifiedPbftBlock(taraxa::PbftBlock const& pbft_block) {
  auto full_node = node_.lock();
  if (!full_node) {
    LOG(log_err_) << "Full node unavailable";
    assert(false);
  }

  blk_hash_t block_hash = pbft_block.getBlockHash();
  blk_hash_t prev_block_hash = pbft_block.getPrevBlockHash();
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
  uniqueLock_ lock(unverified_access_);
  unverified_blocks_[pbft_block.getBlockHash()] = pbft_block;
  LOG(log_deb_) << "Push unverified block " << block_hash
                << ". Pbft unverified blocks size: "
                << unverified_blocks_.size();
}

uint64_t PbftChain::pushDagBlockHash(const taraxa::blk_hash_t& dag_block_hash) {
  std::string dag_block_height_str = db_->getDagBlockHeight(dag_block_hash);
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
  db_->saveDagBlockOrder(std::to_string(max_dag_blocks_height_),
                         dag_block_hash);
  // push DAG block hash into DAG blocks height DB
  // key : dag block hash, value : dag block height>
  // DAG genesis is block height 1
  db_->saveDagBlockHeight(dag_block_hash,
                          std::to_string(max_dag_blocks_height_));
  return max_dag_blocks_height_;
}

std::string PbftChain::getGenesisStr() const {
  std::stringstream strm;
  strm << "[PbftChain]" << std::endl;
  strm << "genesis hash: " << genesis_hash_.toString() << std::endl;
  strm << "size: " << size_ << std::endl;
  strm << "period: " << period_ << std::endl;
  strm << "last pbft block hash: " << last_pbft_block_hash_.toString()
       << std::endl;
  return strm.str();
}

std::string PbftChain::getJsonStr() const {
  ptree tree;
  tree.put("genesis_hash", genesis_hash_.toString());
  tree.put("size", size_);
  tree.put("period", period_);
  tree.put("last_pbft_block_hash", last_pbft_block_hash_.toString());
  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain) {
  strm << pbft_chain.getGenesisStr();
  return strm;
}

uint64_t PbftChain::pbftSyncingHeight() const {
  if (pbft_synced_queue_.empty()) {
    return size_;
  } else {
    return pbftSyncedQueueBack().pbft_blk.getHeight();
  }
}

size_t PbftChain::pbftSyncedQueueSize() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.size();
}

bool PbftChain::pbftSyncedQueueEmpty() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.empty();
}

PbftBlockCert PbftChain::pbftSyncedQueueFront() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.front();
}

PbftBlockCert PbftChain::pbftSyncedQueueBack() const {
  sharedLock_ lock(sync_access_);
  return pbft_synced_queue_.back();
}

void PbftChain::pbftSyncedQueuePopFront() {
  pbftSyncedSetErase_();
  uniqueLock_ lock(sync_access_);
  pbft_synced_queue_.pop_front();
}

void PbftChain::setSyncedPbftBlockIntoQueue(
    PbftBlockCert const& pbft_block_and_votes) {
  LOG(log_inf_) << "Receive pbft block "
                << pbft_block_and_votes.pbft_blk.getBlockHash()
                << " from peer and push into synced queue";
  uniqueLock_ lock(sync_access_);

  // NOTE: We have already checked that block is being added in
  //       order, in taraxa capability

  pbft_synced_queue_.emplace_back(pbft_block_and_votes);
  pbft_synced_set_.insert(pbft_block_and_votes.pbft_blk.getBlockHash());
}

void PbftChain::clearSyncedPbftBlocks() {
  uniqueLock_ lock(sync_access_);
  pbft_synced_queue_.clear();
  pbft_synced_set_.clear();
}

void PbftChain::pbftSyncedSetInsert_(blk_hash_t const& pbft_block_hash) {
  uniqueLock_ lock(sync_access_);
  pbft_synced_set_.insert(pbft_block_hash);
}

void PbftChain::pbftSyncedSetErase_() {
  PbftBlock pbft_block = pbftSyncedQueueFront().pbft_blk;
  uniqueLock_ lock(sync_access_);
  pbft_synced_set_.erase(pbft_block.getBlockHash());
}

void PbftChain::insertPbftBlockIndex_(
    taraxa::blk_hash_t const& pbft_block_hash) {
  db_->savePbftBlockOrder(std::to_string(size_), pbft_block_hash);
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

}  // namespace taraxa
