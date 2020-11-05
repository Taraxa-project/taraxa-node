#include "db_storage.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "rocksdb/utilities/checkpoint.h"
#include "transaction.hpp"
#include "vote.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = std::filesystem;

DbStorage::DbStorage(fs::path const& path,
                     uint32_t db_snapshot_each_n_pbft_block,
                     uint32_t db_max_snapshots, uint32_t db_revert_to_period,
                     addr_t node_addr)
    : path_(path),
      handles_(Columns::all.size()),
      db_snapshot_each_n_pbft_block_(db_snapshot_each_n_pbft_block),
      db_max_snapshots_(db_max_snapshots),
      node_addr_(node_addr) {
  db_path_ = (path / db_dir);
  state_db_path_ = (path / state_db_dir);
  fs::create_directories(db_path_);
  rocksdb::Options options;
  options.create_missing_column_families = true;
  options.create_if_missing = true;
  vector<ColumnFamilyDescriptor> descriptors;
  std::transform(Columns::all.begin(), Columns::all.end(),
                 std::back_inserter(descriptors), [](const Column& col) {
                   return ColumnFamilyDescriptor(col.name,
                                                 ColumnFamilyOptions());
                 });
  LOG_OBJECTS_CREATE("DBS");
  if (db_revert_to_period) {
    recoverToPeriod(db_revert_to_period);
  }
  checkStatus(
      DB::Open(options, db_path_.string(), descriptors, &handles_, &db_));
  dag_blocks_count_.store(getStatusField(StatusDbField::DagBlkCount));
  dag_edge_count_.store(getStatusField(StatusDbField::DagEdgeCount));
}

bool DbStorage::createSnapshot(uint64_t const& period) {
  if (db_snapshot_each_n_pbft_block_ > 0 &&
      period % db_snapshot_each_n_pbft_block_ == 0) {
    LOG(log_nf_) << "Creating DB snapshot on period: " << period;
    rocksdb::Checkpoint* checkpoint;
    auto status = rocksdb::Checkpoint::Create(db_, &checkpoint);
    checkStatus(status);
    auto snapshot_path = db_path_;
    snapshot_path += std::to_string(period);
    status = checkpoint->CreateCheckpoint(snapshot_path.string());
    delete checkpoint;
    checkStatus(status);
    snapshots_counter++;
    if (db_max_snapshots_ && snapshots_counter == db_max_snapshots_ / 5) {
      snapshots_counter = 0;
      deleteSnapshots(
          (period / db_snapshot_each_n_pbft_block_ - db_max_snapshots_) *
              db_snapshot_each_n_pbft_block_,
          false);
    }
    return true;
  }
  return false;
}

void DbStorage::recoverToPeriod(uint64_t const& period) {
  LOG(log_nf_) << "Revet to snapshot from period: " << period;
  auto period_path = db_path_;
  auto period_state_path = state_db_path_;
  period_path += to_string(period);
  period_state_path += to_string(period);
  if (fs::exists(period_path)) {
    LOG(log_dg_) << "Deleting current db/state";
    fs::remove_all(db_path_);
    fs::remove_all(state_db_path_);
    LOG(log_dg_) << "Reverting to period: " << period;
    fs::rename(period_path, db_path_);
    fs::rename(period_state_path, state_db_path_);
    LOG(log_dg_) << "Deleting newer periods:";
    deleteSnapshots(period, true);
  } else {
    LOG(log_er_) << "Period snapshot missing";
  }
}

void DbStorage::deleteSnapshots(uint64_t const& period, bool const& after) {
  if (after) {
    LOG(log_nf_) << "Deleting Snapshots after period " << period;
  } else {
    LOG(log_nf_) << "Deleting Snapshots before period " << period;
  }
  auto period_path = db_path_;
  auto period_state_path = state_db_path_;
  period_path += to_string(period);
  period_state_path += to_string(period);
  for (fs::directory_iterator itr(path_); itr != fs::directory_iterator();
       ++itr) {
    std::string fileName = itr->path().filename().string();
    bool db_dir_found = fileName.find("db") == 0;
    bool state_db_dir_found = fileName.find("state_db") == 0;
    bool delete_dir = false;
    uint64_t dir_period = 0;
    try {
      if (fileName.find(db_dir) == 0 && fileName.size() > db_dir.size()) {
        dir_period = stoi(fileName.substr(db_dir.size()));
      } else if (fileName.find(state_db_dir) == 0 &&
                 fileName.size() > state_db_dir.size()) {
        dir_period = stoi(fileName.substr(state_db_dir.size()));
      } else {
        continue;
      }
    } catch (...) {
      LOG(log_er_) << "Unexpected file: " << fileName;
      continue;
    }
    if ((after && dir_period > period) || (!after && dir_period < period)) {
      fs::remove_all(itr->path());
      LOG(log_dg_) << "Deleted folder: " << fileName;
    }
  }
}

DbStorage::~DbStorage() {
  db_->Close();
  delete db_;
}

void DbStorage::checkStatus(rocksdb::Status const& status) {
  if (status.ok()) return;
  throw DbException(string("Db error. Status code: ") +
                    to_string(status.code()) +
                    " SubCode: " + to_string(status.subcode()) +
                    " Message:" + status.ToString());
}

DbStorage::BatchPtr DbStorage::createWriteBatch() {
  return s_ptr(new WriteBatch());
}

void DbStorage::remove(Slice key, Column const& column) {
  auto status = db_->Delete(write_options_, handle(column), key);
  checkStatus(status);
}

void DbStorage::commitWriteBatch(BatchPtr const& write_batch) {
  auto status = db_->Write(write_options_, write_batch->GetWriteBatch());
  checkStatus(status);
}

dev::bytes DbStorage::getDagBlockRaw(blk_hash_t const& hash) {
  return asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_blocks));
}

std::shared_ptr<DagBlock> DbStorage::getDagBlock(blk_hash_t const& hash) {
  auto blk_bytes = getDagBlockRaw(hash);
  if (blk_bytes.size() > 0) {
    return std::make_shared<DagBlock>(blk_bytes);
  }
  return nullptr;
}

std::string DbStorage::getBlocksByLevel(level_t level) {
  return lookup(toSlice(level), Columns::dag_blocks_index);
}

std::vector<std::shared_ptr<DagBlock>> DbStorage::getDagBlocksAtLevel(
    level_t level, int number_of_levels) {
  std::vector<std::shared_ptr<DagBlock>> res;
  for (int i = 0; i < number_of_levels; i++) {
    if (level + i == 0) continue;  // Skip genesis
    string entry = getBlocksByLevel(level + i);
    if (entry.empty()) break;
    vector<string> blocks;
    boost::split(blocks, entry, boost::is_any_of(","));
    for (auto const& block : blocks) {
      auto blk = getDagBlock(blk_hash_t(block));
      if (blk) {
        res.push_back(blk);
      }
    }
  }
  return res;
}

void DbStorage::saveDagBlock(DagBlock const& blk, BatchPtr write_batch) {
  // Lock is needed since we are editing some fields
  lock_guard<mutex> u_lock(dag_blocks_mutex_);
  bool commit = false;
  if (write_batch == nullptr) {
    write_batch = createWriteBatch();
    commit = true;
  }
  auto block_bytes = blk.rlp(true);
  auto block_hash = blk.getHash();
  batch_put(write_batch, Columns::dag_blocks, toSlice(block_hash.asBytes()),
            toSlice(block_bytes));
  auto level = blk.getLevel();
  std::string blocks = getBlocksByLevel(level);
  if (blocks == "") {
    blocks = blk.getHash().toString();
  } else {
    blocks = blocks + "," + blk.getHash().toString();
  }
  batch_put(write_batch, Columns::dag_blocks_index, toSlice(level),
            toSlice(blocks));
  dag_blocks_count_.fetch_add(1);
  batch_put(write_batch, Columns::status,
            toSlice((uint8_t)StatusDbField::DagBlkCount),
            toSlice(dag_blocks_count_.load()));
  // Do not count genesis pivot field
  if (blk.getPivot() == blk_hash_t(0)) {
    dag_edge_count_.fetch_add(blk.getTips().size());
  } else {
    dag_edge_count_.fetch_add(blk.getTips().size() + 1);
  }
  batch_put(write_batch, Columns::status,
            toSlice((uint8_t)StatusDbField::DagEdgeCount),
            toSlice(dag_edge_count_.load()));
  if (commit) {
    commitWriteBatch(write_batch);
  }
}

void DbStorage::addDagBlockStateToBatch(BatchPtr const& write_batch,
                                        blk_hash_t const& blk_hash,
                                        bool finalized) {
  batch_put(write_batch, Columns::dag_blocks_state, toSlice(blk_hash.asBytes()),
            toSlice(finalized));
}

void DbStorage::removeDagBlockStateToBatch(BatchPtr const& write_batch,
                                           blk_hash_t const& blk_hash) {
  batch_delete(write_batch, Columns::dag_blocks_state,
               toSlice(blk_hash.asBytes()));
}

std::map<blk_hash_t, bool> DbStorage::getAllDagBlockState() {
  std::map<blk_hash_t, bool> res;
  auto i =
      u_ptr(db_->NewIterator(read_options_, handle(Columns::dag_blocks_state)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    res[blk_hash_t(asBytes(i->key().ToString()))] =
        (bool)*(uint8_t*)(i->value().data());
  }
  return res;
}

void DbStorage::saveTransaction(Transaction const& trx) {
  insert(Columns::transactions, toSlice(trx.getHash().asBytes()),
         toSlice(*trx.rlp()));
}

void DbStorage::saveTransactionStatus(trx_hash_t const& trx_hash,
                                      TransactionStatus const& status) {
  insert(Columns::trx_status, toSlice(trx_hash.asBytes()),
         toSlice((uint16_t)status));
}

void DbStorage::addTransactionStatusToBatch(BatchPtr const& write_batch,
                                            trx_hash_t const& trx,
                                            TransactionStatus const& status) {
  batch_put(write_batch, Columns::trx_status, toSlice(trx.asBytes()),
            toSlice((uint16_t)status));
}

TransactionStatus DbStorage::getTransactionStatus(trx_hash_t const& hash) {
  auto data = lookup(toSlice(hash.asBytes()), Columns::trx_status);
  if (!data.empty()) {
    return (TransactionStatus) * (uint16_t*)&data[0];
  }
  return TransactionStatus::not_seen;
}

std::map<trx_hash_t, TransactionStatus> DbStorage::getAllTransactionStatus() {
  std::map<trx_hash_t, TransactionStatus> res;
  auto i = u_ptr(db_->NewIterator(read_options_, handle(Columns::trx_status)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    res[trx_hash_t(asBytes(i->key().ToString()))] =
        (TransactionStatus) * (uint16_t*)(i->value().data());
  }
  return res;
}

dev::bytes DbStorage::getTransactionRaw(trx_hash_t const& hash) {
  return asBytes(lookup(toSlice(hash.asBytes()), Columns::transactions));
}

std::shared_ptr<Transaction> DbStorage::getTransaction(trx_hash_t const& hash) {
  auto trx_bytes = getTransactionRaw(hash);
  if (trx_bytes.size() > 0) {
    return std::make_shared<Transaction>(trx_bytes);
  }
  return nullptr;
}

std::shared_ptr<std::pair<Transaction, taraxa::bytes>>
DbStorage::getTransactionExt(trx_hash_t const& hash) {
  auto trx_bytes =
      asBytes(lookup(toSlice(hash.asBytes()), Columns::transactions));
  if (trx_bytes.size() > 0) {
    return std::make_shared<std::pair<Transaction, taraxa::bytes>>(trx_bytes,
                                                                   trx_bytes);
  }
  return nullptr;
}

void DbStorage::addTransactionToBatch(Transaction const& trx,
                                      BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::transactions,
            toSlice(trx.getHash().asBytes()), toSlice(*trx.rlp()));
}

bool DbStorage::transactionInDb(trx_hash_t const& hash) {
  return !lookup(toSlice(hash.asBytes()), Columns::transactions).empty();
}

uint64_t DbStorage::getStatusField(StatusDbField const& field) {
  auto status = lookup(toSlice((uint8_t)field), Columns::status);
  if (!status.empty()) return *(uint64_t*)&status[0];
  return 0;
}

void DbStorage::saveStatusField(StatusDbField const& field,
                                uint64_t const& value) {
  insert(Columns::status, toSlice((uint8_t)field), toSlice(value));
}

void DbStorage::addStatusFieldToBatch(StatusDbField const& field,
                                      uint64_t const& value,
                                      BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::status, toSlice((uint8_t)field),
            toSlice(value));
}

// PBFT
std::shared_ptr<PbftBlock> DbStorage::getPbftBlock(blk_hash_t const& hash) {
  auto block = lookup(hash, Columns::pbft_blocks);
  if (!block.empty()) {
    return s_ptr(new PbftBlock(dev::RLP(block)));
  }
  return nullptr;
}

bool DbStorage::pbftBlockInDb(blk_hash_t const& hash) {
  return !lookup(hash, Columns::pbft_blocks).empty();
}

void DbStorage::addPbftBlockToBatch(
    const taraxa::PbftBlock& pbft_block,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(*write_batch, Columns::pbft_blocks, pbft_block.getBlockHash(),
            pbft_block.rlp(true));
}

string DbStorage::getPbftHead(blk_hash_t const& hash) {
  return lookup(toSlice(hash.asBytes()), Columns::pbft_head);
}

void DbStorage::savePbftHead(blk_hash_t const& hash,
                             string const& pbft_chain_head_str) {
  insert(Columns::pbft_head, toSlice(hash.asBytes()), pbft_chain_head_str);
}

void DbStorage::addPbftHeadToBatch(
    taraxa::blk_hash_t const& head_hash, std::string const& head_str,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::pbft_head, toSlice(head_hash.asBytes()),
            head_str);
}

bytes DbStorage::getVotes(blk_hash_t const& hash) {
  return asBytes(lookup(hash, Columns::votes));
}

void DbStorage::addPbftCertVotesToBatch(
    const taraxa::blk_hash_t& pbft_block_hash,
    const std::vector<Vote>& cert_votes,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  RLPStream s(cert_votes.size());
  for (auto const& v : cert_votes) {
    s.appendRaw(v.rlp());
  }
  batch_put(*write_batch, Columns::votes, pbft_block_hash, s.out());
}

shared_ptr<blk_hash_t> DbStorage::getPeriodPbftBlock(uint64_t const& period) {
  auto hash = asBytes(lookup(toSlice(period), Columns::period_pbft_block));
  if (hash.size() > 0) {
    return make_shared<blk_hash_t>(hash);
  }
  return nullptr;
}

void DbStorage::addPbftBlockPeriodToBatch(
    uint64_t const& period, taraxa::blk_hash_t const& pbft_block_hash,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::period_pbft_block, toSlice(period),
            toSlice(pbft_block_hash.asBytes()));
}

std::shared_ptr<uint64_t> DbStorage::getDagBlockPeriod(blk_hash_t const& hash) {
  auto period =
      asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_block_period));
  if (period.size() > 0) {
    return make_shared<uint64_t>(*(uint64_t*)&period[0]);
  }
  return nullptr;
}

void DbStorage::addDagBlockPeriodToBatch(blk_hash_t const& hash,
                                         uint64_t const& period,
                                         BatchPtr const& write_batch) {
  batch_put(write_batch, Columns::dag_block_period, toSlice(hash.asBytes()),
            toSlice(period));
}

vector<blk_hash_t> DbStorage::getFinalizedDagBlockHashesByAnchor(
    blk_hash_t const& anchor) {
  auto raw = lookup(toSlice(anchor), Columns::dag_finalized_blocks);
  if (raw.empty()) {
    return {};
  }
  return RLP(raw).toVector<blk_hash_t>();
}

void DbStorage::putFinalizedDagBlockHashesByAnchor(
    WriteBatch& b, blk_hash_t const& anchor, vector<blk_hash_t> const& hs) {
  batch_put(b, DbStorage::Columns::dag_finalized_blocks, anchor,
            RLPStream().appendVector(hs).out());
}

void DbStorage::insert(Column const& col, Slice const& k, Slice const& v) {
  checkStatus(db_->Put(write_options_, handle(col), k, v));
}

void DbStorage::forEach(Column const& col, OnEntry const& f) {
  auto i = u_ptr(db_->NewIterator(read_options_, handle(col)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    if (!f(i->key(), i->value())) {
      break;
    }
  }
}

DbStorage::MultiGetQuery::MultiGetQuery(shared_ptr<DbStorage> const& db,
                                        uint capacity)
    : db_(db) {
  if (capacity) {
    cfs_.reserve(capacity);
    keys_.reserve(capacity);
    str_pool_.reserve(capacity);
  }
}

dev::bytesConstRef DbStorage::MultiGetQuery::get_key(uint pos) {
  auto const& slice = keys_[pos];
  return dev::bytesConstRef((uint8_t*)slice.data(), slice.size());
}

uint DbStorage::MultiGetQuery::size() { return keys_.size(); }

vector<string> DbStorage::MultiGetQuery::execute(bool and_reset) {
  auto _size = size();
  if (_size == 0) {
    return {};
  }
  vector<string> ret(_size);
  uint i = 0;
  for (auto const& s :
       db_->db_->MultiGet(db_->read_options_, cfs_, keys_, &ret)) {
    if (s.IsNotFound()) {
      ret[i] = "";
    } else {
      checkStatus(s);
    }
    ++i;
  }
  if (and_reset) {
    reset();
  }
  return ret;
}

DbStorage::MultiGetQuery& DbStorage::MultiGetQuery::reset() {
  cfs_.clear();
  keys_.clear();
  str_pool_.clear();
  return *this;
}

}  // namespace taraxa
