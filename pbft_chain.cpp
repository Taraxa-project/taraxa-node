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
  streamRLP(s);
  return s.out();
}

void TrxSchedule::streamRLP(dev::RLPStream& s) const {
  uint64_t num_blk = dag_blks_order.size();
  std::vector<size_t> trx_in_each_blk;
  uint64_t num_trx = 0;
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
    uint64_t trx_size = trxs_mode[i].size();
    s << trx_size;
    for (int j(0); j < trx_size; ++j) {
      s << trxs_mode[i][j].first;
      s << trxs_mode[i][j].second;
    }
  }
}

TrxSchedule::TrxSchedule(dev::RLP const& r) {
  dev::RLP const rlp(r);
  if (!rlp.isList())
    throw std::invalid_argument("TrxSchedule RLP must be a list");

  auto num_blk = rlp[0].toInt<uint64_t>();
  for (auto i(0); i < num_blk; ++i) {
    dag_blks_order.push_back(rlp[i + 1].toHash<blk_hash_t>());
  }
  int counter = num_blk;
  for (int i(0); i < num_blk; ++i) {
    counter++;
    auto trx_size = rlp[counter].toInt<uint64_t>();
    std::vector<std::pair<trx_hash_t, uint>> temp_v;
    for (int j(0); j < trx_size; ++j) {
      counter++;
      auto first = rlp[counter].toHash<trx_hash_t>();
      counter++;
      auto second = rlp[counter].toInt<int32_t>();
      temp_v.push_back(std::make_pair(first, second));
    }
    trxs_mode.push_back(temp_v);
  }
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
  dev::RLP const rlp(r);
  if (!rlp.isList()) throw std::invalid_argument("PBFT RLP must be a list");
  prev_block_hash_ = rlp[0].toHash<blk_hash_t>();
  dag_block_hash_as_pivot_ = rlp[1].toHash<blk_hash_t>();
  period_ = rlp[2].toInt<int64_t>();
  height_ = rlp[3].toInt<int64_t>();
  timestamp_ = rlp[4].toInt<int64_t>();
  signature_ = rlp[5].toHash<sig_t>();
  schedule_ = TrxSchedule(rlp[6]);
  calculateHash_();
}

PbftBlock::PbftBlock(blk_hash_t const& prev_blk_hash,
                     blk_hash_t const& dag_blk_hash_as_pivot,
                     TrxSchedule const& schedule, uint64_t period,
                     uint64_t height, addr_t const& beneficiary,
                     secret_t const& sk)
    : prev_block_hash_(prev_blk_hash),
      dag_block_hash_as_pivot_(dag_blk_hash_as_pivot),
      schedule_(schedule),
      period_(period),
      height_(height),
      beneficiary_(beneficiary) {
  timestamp_ = dev::utcTime();
  signature_ = dev::sign(sk, sha3(false));
  calculateHash_();
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
  signature_ = sig_t(doc.get<std::string>("signature"));
  beneficiary_ = addr_t(doc.get<std::string>("beneficiary"));
}

void PbftBlock::calculateHash_() {
  if (!block_hash_) {
    block_hash_ = dev::sha3(rlp(true));
  } else {
    // Hash sould only be calculated once
    assert(false);
  }
  auto p = dev::recover(signature_, sha3(false));
  assert(p);
  beneficiary_ =
      dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
}

blk_hash_t PbftBlock::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

std::string PbftBlock::getJsonStr() const {
  ptree tree;
  tree.put("prev_block_hash", prev_block_hash_.toString());
  tree.put("dag_block_hash_as_pivot", dag_block_hash_as_pivot_.toString());
  tree.put_child("schedule", ptree());
  schedule_.setPtree(tree.get_child("schedule"));
  tree.put("period", period_);
  tree.put("height", height_);
  tree.put("timestamp", timestamp_);
  tree.put("block_hash", block_hash_.toString());
  tree.put("signature", signature_.toString());
  tree.put("beneficiary", beneficiary_);

  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

addr_t PbftBlock::getBeneficiary() const { return beneficiary_; }

// Using to setup PBFT block hash
void PbftBlock::streamRLP(dev::RLPStream& strm, bool include_sig) const {
  strm.appendList(include_sig ? 7 : 6);
  strm << prev_block_hash_;
  strm << dag_block_hash_as_pivot_;
  strm << period_;
  strm << height_;
  strm << timestamp_;
  if (include_sig) strm << signature_;
  schedule_.streamRLP(strm);
}

bytes PbftBlock::rlp(bool include_sig) const {
  RLPStream strm;
  streamRLP(strm, include_sig);
  return strm.out();
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
  s.append(pbft_blk.rlp(true));
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
    : head_hash_(blk_hash_t(0)),
      size_(0),
      period_(0),
      last_pbft_block_hash_(head_hash_),
      dag_genesis_hash_(blk_hash_t(dag_genesis_hash)) {}

void PbftChain::setFullNode(std::shared_ptr<taraxa::FullNode> full_node) {
  node_ = full_node;
  // setup pbftchain DB pointer
  db_ = full_node->getDB();
  assert(db_);

  // Get PBFT head from DB
  auto pbft_head_str = db_->getPbftHead(head_hash_);
  if (pbft_head_str.empty()) {
    // Store PBFT HEAD to db
    db_->savePbftHead(head_hash_, getJsonStr());
    // Initialize DAG genesis at DAG block heigh 1
    pushDagBlockHash(dag_genesis_hash_);
  } else {
    // set PBFT HEAD from DB
    setPbftHead(pbft_head_str);
  }
}

void PbftChain::setPbftHead(std::string const& pbft_head_str) {
  ptree doc = strToJson(pbft_head_str);

  head_hash_ = blk_hash_t(doc.get<std::string>("head_hash"));
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
  auto dag_block_hash = db_->getDagBlockOrder(dag_block_height);
  if (!dag_block_hash) {
    LOG(log_err_) << "The DAG block height " << dag_block_height
                  << " is not exist in DAG blocks order DB.";
    return std::make_pair(blk_hash_t(0), false);
  }
  return std::make_pair(*dag_block_hash, true);
}

// TODO: should remove, full node should call db directly
std::pair<uint64_t, bool> PbftChain::getDagBlockHeight(
    blk_hash_t const& dag_block_hash) const {
  auto dag_block_height_ptr = db_->getDagBlockHeight(dag_block_hash);
  if (!dag_block_height_ptr) {
    LOG(log_err_) << "Cannot find the DAG block hash " << dag_block_hash
                  << " in DAG blocks height DB";
    return std::make_pair(0, false);
  }
  return std::make_pair(*dag_block_height_ptr, true);
}

uint64_t PbftChain::getDagBlockMaxHeight() const {
  return max_dag_blocks_height_;
}

void PbftChain::setDagBlockMaxHeight(uint64_t const& max_dag_blocks_height) {
  max_dag_blocks_height_ = max_dag_blocks_height;
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
    auto pbft_block_hash = db_->getPbftBlockOrder(i);
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
    auto pbft_block_hash = db_->getPbftBlockOrder(i);
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

void PbftChain::updatePbftChain(blk_hash_t const& pbft_block_hash) {
  pbftSyncedSetInsert_(pbft_block_hash);
  period_++;
  size_++;
  setLastPbftBlockHash(pbft_block_hash);
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
  auto dag_block_height_ptr = db_->getDagBlockHeight(dag_block_hash);
  if (dag_block_height_ptr) {
    // The DAG block already exist
    LOG(log_inf_) << "Duplicate DAG block " << dag_block_hash
                  << " in DAG blocks height DB already";
    return *dag_block_height_ptr;
  }
  // push DAG block hash into DAG blocks order DB. DAG genesis at index 1
  max_dag_blocks_height_++;
  db_->saveDagBlockOrder(max_dag_blocks_height_, dag_block_hash);
  // push DAG block hash into DAG blocks height DB
  // key : dag block hash, value : dag block height
  // DAG genesis is block height 1
  db_->saveDagBlockHeight(dag_block_hash, max_dag_blocks_height_);
  return max_dag_blocks_height_;
}

std::string PbftChain::getHeadStr() const {
  std::stringstream strm;
  strm << "[PbftChain]" << std::endl;
  strm << "head hash: " << head_hash_.toString() << std::endl;
  strm << "size: " << size_ << std::endl;
  strm << "period: " << period_ << std::endl;
  strm << "last pbft block hash: " << last_pbft_block_hash_.toString()
       << std::endl;
  return strm.str();
}

std::string PbftChain::getJsonStr() const {
  ptree tree;
  tree.put("head_hash", head_hash_.toString());
  tree.put("size", size_);
  tree.put("period", period_);
  tree.put("last_pbft_block_hash", last_pbft_block_hash_.toString());
  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

std::ostream& operator<<(std::ostream& strm, PbftChain const& pbft_chain) {
  strm << pbft_chain.getHeadStr();
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
