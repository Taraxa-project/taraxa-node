#include "storage/storage.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "config/version.hpp"
#include "dag/sortition_params_manager.hpp"
#include "rocksdb/utilities/checkpoint.h"
#include "storage/uint_comparator.hpp"
#include "vote/vote.hpp"

namespace taraxa {
namespace fs = std::filesystem;

static constexpr uint16_t PBFT_BLOCK_POS_IN_PERIOD_DATA = 0;
static constexpr uint16_t CERT_VOTES_POS_IN_PERIOD_DATA = 1;
static constexpr uint16_t DAG_BLOCKS_POS_IN_PERIOD_DATA = 2;
static constexpr uint16_t TRANSACTIONS_POS_IN_PERIOD_DATA = 3;

DbStorage::DbStorage(fs::path const& path, uint32_t db_snapshot_each_n_pbft_block, uint32_t max_open_files,
                     uint32_t db_max_snapshots, uint32_t db_revert_to_period, addr_t node_addr, bool rebuild,
                     bool rebuild_columns)
    : path_(path),
      handles_(Columns::all.size()),
      db_snapshot_each_n_pbft_block_(db_snapshot_each_n_pbft_block),
      db_max_snapshots_(db_max_snapshots) {
  db_path_ = (path / db_dir);
  state_db_path_ = (path / state_db_dir);

  if (rebuild) {
    const std::string backup_label = "-rebuild-backup-";
    auto timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    auto backup_db_path = db_path_;
    backup_db_path += (backup_label + timestamp);
    auto backup_state_db_path = state_db_path_;
    backup_state_db_path += (backup_label + timestamp);
    fs::rename(db_path_, backup_db_path);
    fs::rename(state_db_path_, backup_state_db_path);
    db_path_ = backup_db_path;
    state_db_path_ = backup_state_db_path;
  }

  fs::create_directories(db_path_);
  rocksdb::Options options;
  options.create_missing_column_families = true;
  options.create_if_missing = true;
  options.compression = rocksdb::CompressionType::kLZ4Compression;
  // This options is related to memory consuption
  // https://github.com/facebook/rocksdb/issues/3216#issuecomment-817358217
  // aleth default 256 (state_db is using another 128)
  options.max_open_files = (max_open_files) ? max_open_files : 256;

  std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
  descriptors.reserve(Columns::all.size());
  std::transform(Columns::all.begin(), Columns::all.end(), std::back_inserter(descriptors), [](const Column& col) {
    auto options = rocksdb::ColumnFamilyOptions();
    if (col.comparator_) options.comparator = col.comparator_;
    return rocksdb::ColumnFamilyDescriptor(col.name(), options);
  });
  LOG_OBJECTS_CREATE("DBS");

  if (rebuild_columns) {
    rebuildColumns(options);
  }

  // Iterate over the db folders and populate snapshot set
  loadSnapshots();

  // Revert to period if needed
  if (db_revert_to_period) {
    recoverToPeriod(db_revert_to_period);
  }
  rocksdb::DB* db = nullptr;
  checkStatus(rocksdb::DB::Open(options, db_path_.string(), descriptors, &handles_, &db));
  assert(db);
  db_.reset(db);
  dag_blocks_count_.store(getStatusField(StatusDbField::DagBlkCount));
  dag_edge_count_.store(getStatusField(StatusDbField::DagEdgeCount));

  uint32_t major_version = getStatusField(StatusDbField::DbMajorVersion);
  uint32_t minor_version = getStatusField(StatusDbField::DbMinorVersion);
  if (major_version == 0 && minor_version == 0) {
    saveStatusField(StatusDbField::DbMajorVersion, TARAXA_DB_MAJOR_VERSION);
    saveStatusField(StatusDbField::DbMinorVersion, TARAXA_DB_MINOR_VERSION);
  } else {
    if (major_version != TARAXA_DB_MAJOR_VERSION) {
      throw DbException(string("Database version mismatch. Version on disk ") +
                        getFormattedVersion({major_version, minor_version}) +
                        " Node version:" + getFormattedVersion({TARAXA_DB_MAJOR_VERSION, TARAXA_DB_MINOR_VERSION}));
    } else if (minor_version != TARAXA_DB_MINOR_VERSION) {
      minor_version_changed_ = true;
    }
  }
}

void DbStorage::rebuildColumns(const rocksdb::Options& options) {
  std::unique_ptr<rocksdb::DB> db;
  std::vector<std::string> column_families;
  rocksdb::DB::ListColumnFamilies(options, db_path_.string(), &column_families);

  std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
  descriptors.reserve(column_families.size());
  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  handles.reserve(column_families.size());
  std::transform(column_families.begin(), column_families.end(), std::back_inserter(descriptors), [](const auto& name) {
    return rocksdb::ColumnFamilyDescriptor(name, rocksdb::ColumnFamilyOptions());
  });
  rocksdb::DB* db_ptr = nullptr;
  checkStatus(rocksdb::DB::Open(options, db_path_.string(), descriptors, &handles, &db_ptr));
  assert(db_ptr);
  db.reset(db_ptr);
  for (size_t i = 0; i < handles.size(); ++i) {
    const auto it = std::find_if(Columns::all.begin(), Columns::all.end(),
                                 [&handles, i](const Column& col) { return col.name() == handles[i]->GetName(); });
    if (it == Columns::all.end()) {
      LOG(log_si_) << "Removing column: " << handles[i]->GetName();
      checkStatus(db->DropColumnFamily(handles[i]));
    }
  }
  for (auto cf : handles) {
    checkStatus(db->DestroyColumnFamilyHandle(cf));
  }
  checkStatus(db->Close());
}

void DbStorage::loadSnapshots() {
  // Find all the existing folders containing db and state_db snapshots
  for (fs::directory_iterator itr(path_); itr != fs::directory_iterator(); ++itr) {
    std::string fileName = itr->path().filename().string();
    uint64_t dir_period = 0;

    try {
      // Check for db or state_db prefix
      if (boost::starts_with(fileName, db_dir) && fileName.size() > db_dir.size()) {
        dir_period = stoi(fileName.substr(db_dir.size()));
      } else if (boost::starts_with(fileName, state_db_dir) && fileName.size() > state_db_dir.size()) {
        dir_period = stoi(fileName.substr(state_db_dir.size()));
      } else {
        continue;
      }
    } catch (...) {
      // This should happen if there is a incorrect formatted folder, just skip
      // it
      LOG(log_er_) << "Unexpected file: " << fileName;
      continue;
    }
    LOG(log_dg_) << "Found snapshot: " << fileName;
    snapshots_.insert(dir_period);
  }
}

bool DbStorage::createSnapshot(uint64_t period) {
  // Only creates snapshot each db_snapshot_each_n_pbft_block_ periods
  if (!snapshot_enable_ || db_snapshot_each_n_pbft_block_ <= 0 || period % db_snapshot_each_n_pbft_block_ != 0 ||
      snapshots_.find(period) != snapshots_.end()) {
    return false;
  }

  LOG(log_nf_) << "Creating DB snapshot on period: " << period;

  // Create rocskd checkpoint/snapshot
  rocksdb::Checkpoint* checkpoint;
  auto status = rocksdb::Checkpoint::Create(db_.get(), &checkpoint);
  // Scope is to delete checkpoint object as soon as we don't need it anymore
  {
    checkStatus(status);
    auto snapshot_path = db_path_;
    snapshot_path += std::to_string(period);
    status = checkpoint->CreateCheckpoint(snapshot_path.string());
  }
  checkStatus(status);
  snapshots_.insert(period);

  // Delete any snapshot over db_max_snapshots_
  if (db_max_snapshots_ && snapshots_.size() > db_max_snapshots_) {
    while (snapshots_.size() > db_max_snapshots_) {
      auto snapshot = snapshots_.begin();
      deleteSnapshot(*snapshot);
      snapshots_.erase(snapshot);
    }
  }
  return true;
}

void DbStorage::recoverToPeriod(uint64_t period) {
  LOG(log_nf_) << "Revet to snapshot from period: " << period;

  // Construct the snapshot folder names
  auto period_path = db_path_;
  auto period_state_path = state_db_path_;
  period_path += std::to_string(period);
  period_state_path += std::to_string(period);

  if (fs::exists(period_path)) {
    LOG(log_dg_) << "Deleting current db/state";
    fs::remove_all(db_path_);
    fs::remove_all(state_db_path_);
    LOG(log_dg_) << "Reverting to period: " << period;
    fs::rename(period_path, db_path_);
    fs::rename(period_state_path, state_db_path_);
    // Delete all the period snapshots that are after the snapshot we are
    // reverting to
    LOG(log_dg_) << "Deleting newer periods:";
    for (auto snapshot = snapshots_.begin(); snapshot != snapshots_.end();) {
      if (*snapshot > period) {
        deleteSnapshot(*snapshot);
        snapshots_.erase(snapshot++);
      } else {
        snapshot++;
      }
    }
  } else {
    LOG(log_er_) << "Period snapshot missing";
  }
}

void DbStorage::deleteSnapshot(uint64_t period) {
  LOG(log_nf_) << "Deleting " << period << "snapshot";

  // Construct the snapshot folder names
  auto period_path = db_path_;
  auto period_state_path = state_db_path_;
  period_path += std::to_string(period);
  period_state_path += std::to_string(period);

  // Delete both db and state_db folder
  fs::remove_all(period_path);
  LOG(log_dg_) << "Deleted folder: " << period_path;
  fs::remove_all(period_state_path);
  LOG(log_dg_) << "Deleted folder: " << period_path;
}

void DbStorage::disableSnapshots() { snapshot_enable_ = false; }

void DbStorage::enableSnapshots() { snapshot_enable_ = true; }

DbStorage::~DbStorage() {
  for (auto cf : handles_) {
    checkStatus(db_->DestroyColumnFamilyHandle(cf));
  }
  checkStatus(db_->Close());
}

void DbStorage::checkStatus(rocksdb::Status const& status) {
  if (status.ok()) return;
  throw DbException(string("Db error. Status code: ") + std::to_string(status.code()) +
                    " SubCode: " + std::to_string(status.subcode()) + " Message:" + status.ToString());
}

DbStorage::Batch DbStorage::createWriteBatch() { return DbStorage::Batch(); }

void DbStorage::commitWriteBatch(Batch& write_batch, rocksdb::WriteOptions const& opts) {
  auto status = db_->Write(opts, write_batch.GetWriteBatch());
  checkStatus(status);
}

std::shared_ptr<DagBlock> DbStorage::getDagBlock(blk_hash_t const& hash) {
  auto block_data = asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_blocks));
  if (block_data.size() > 0) {
    return std::make_shared<DagBlock>(block_data);
  }
  auto data = getDagBlockPeriod(hash);
  if (data) {
    auto period_data = getPeriodDataRaw(data->first);
    if (period_data.size() > 0) {
      auto period_data_rlp = dev::RLP(period_data);
      auto dag_blocks_data = period_data_rlp[DAG_BLOCKS_POS_IN_PERIOD_DATA];
      return std::make_shared<DagBlock>(dag_blocks_data[data->second]);
    }
  }
  return nullptr;
}

bool DbStorage::dagBlockInDb(blk_hash_t const& hash) {
  if (exist(toSlice(hash.asBytes()), Columns::dag_blocks) ||
      exist(toSlice(hash.asBytes()), Columns::dag_block_period)) {
    return true;
  }
  return false;
}

std::set<blk_hash_t> DbStorage::getBlocksByLevel(level_t level) {
  auto data = asBytes(lookup(toSlice(level), Columns::dag_blocks_index));
  dev::RLP rlp(data);
  return rlp.toSet<blk_hash_t>();
}

std::vector<std::shared_ptr<DagBlock>> DbStorage::getDagBlocksAtLevel(level_t level, int number_of_levels) {
  std::vector<std::shared_ptr<DagBlock>> res;
  for (int i = 0; i < number_of_levels; i++) {
    if (level + i == 0) continue;  // Skip genesis
    auto block_hashes = getBlocksByLevel(level + i);
    for (auto const& hash : block_hashes) {
      auto blk = getDagBlock(hash);
      if (blk) {
        res.push_back(blk);
      }
    }
  }
  return res;
}

std::map<level_t, std::vector<DagBlock>> DbStorage::getNonfinalizedDagBlocks() {
  std::map<level_t, std::vector<DagBlock>> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::dag_blocks)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    DagBlock block(asBytes(i->value().ToString()));
    if (block.getPivot() != blk_hash_t(0)) {
      res[block.getLevel()].emplace_back(std::move(block));
    }
  }
  return res;
}

SharedTransactions DbStorage::getAllNonfinalizedTransactions() {
  SharedTransactions res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::transactions)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    Transaction transaction(asBytes(i->value().ToString()));
    res.emplace_back(std::make_shared<Transaction>(std::move(transaction)));
  }
  return res;
}

void DbStorage::removeDagBlockBatch(Batch& write_batch, blk_hash_t const& hash) {
  remove(write_batch, Columns::dag_blocks, toSlice(hash));
}

void DbStorage::removeDagBlock(blk_hash_t const& hash) { remove(Columns::dag_blocks, toSlice(hash)); }

void DbStorage::updateDagBlockCounters(std::vector<DagBlock> blks) {
  // Lock is needed since we are editing some fields
  std::lock_guard<std::mutex> u_lock(dag_blocks_mutex_);
  auto write_batch = createWriteBatch();
  for (auto const& blk : blks) {
    auto level = blk.getLevel();
    auto block_hashes = getBlocksByLevel(level);
    block_hashes.emplace(blk.getHash());
    dev::RLPStream blocks_stream(block_hashes.size());
    for (auto const& hash : block_hashes) {
      blocks_stream << hash;
    }
    insert(write_batch, Columns::dag_blocks_index, toSlice(level), toSlice(blocks_stream.out()));
    dag_blocks_count_.fetch_add(1);
    // Do not count genesis pivot field
    if (blk.getPivot() == blk_hash_t(0)) {
      dag_edge_count_.fetch_add(blk.getTips().size());
    } else {
      dag_edge_count_.fetch_add(blk.getTips().size() + 1);
    }
  }
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagBlkCount), toSlice(dag_blocks_count_.load()));
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagEdgeCount), toSlice(dag_edge_count_.load()));
  commitWriteBatch(write_batch);
}

void DbStorage::saveDagBlock(DagBlock const& blk, Batch* write_batch_p) {
  // Lock is needed since we are editing some fields
  std::lock_guard<std::mutex> u_lock(dag_blocks_mutex_);
  auto write_batch_up = write_batch_p ? std::unique_ptr<Batch>() : std::make_unique<Batch>();
  auto commit = !write_batch_p;
  auto& write_batch = write_batch_p ? *write_batch_p : *write_batch_up;
  auto block_bytes = blk.rlp(true);
  auto block_hash = blk.getHash();
  insert(write_batch, Columns::dag_blocks, toSlice(block_hash.asBytes()), toSlice(block_bytes));
  auto level = blk.getLevel();
  auto block_hashes = getBlocksByLevel(level);
  block_hashes.emplace(blk.getHash());
  dev::RLPStream blocks_stream(block_hashes.size());
  for (auto const& hash : block_hashes) {
    blocks_stream << hash;
  }
  insert(write_batch, Columns::dag_blocks_index, toSlice(level), toSlice(blocks_stream.out()));
  dag_blocks_count_.fetch_add(1);
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagBlkCount), toSlice(dag_blocks_count_.load()));
  // Do not count genesis pivot field
  if (blk.getPivot() == blk_hash_t(0)) {
    dag_edge_count_.fetch_add(blk.getTips().size());
  } else {
    dag_edge_count_.fetch_add(blk.getTips().size() + 1);
  }
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagEdgeCount), toSlice(dag_edge_count_.load()));
  if (commit) {
    commitWriteBatch(write_batch);
  }
}

// Sortition params
void DbStorage::saveSortitionParamsChange(uint64_t period, const SortitionParamsChange& params, Batch& batch) {
  insert(batch, Columns::sortition_params_change, toSlice(period), toSlice(params.rlp()));
}

std::deque<SortitionParamsChange> DbStorage::getLastSortitionParams(size_t count) {
  std::deque<SortitionParamsChange> changes;

  auto it =
      std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::sortition_params_change)));
  for (it->SeekToLast(); it->Valid() && changes.size() < count; it->Prev()) {
    changes.push_front(SortitionParamsChange::from_rlp(dev::RLP(it->value().ToString())));
  }

  return changes;
}

std::optional<SortitionParamsChange> DbStorage::getParamsChangeForPeriod(uint64_t period) {
  auto it =
      std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::sortition_params_change)));
  it->SeekForPrev(toSlice(period));

  // this means that no sortition_params_change in database. It could be met in the tests
  if (!it->Valid()) {
    return {};
  }

  return SortitionParamsChange::from_rlp(dev::RLP(it->value().ToString()));
}

void DbStorage::clearPeriodDataHistory(uint64_t period) {
  const uint64_t start = 0;
  db_->DeleteRange(write_options_, handle(Columns::period_data), toSlice(start), toSlice(period));
}

void DbStorage::savePeriodData(const SyncBlock& sync_block, Batch& write_batch) {
  uint64_t period = sync_block.pbft_blk->getPeriod();
  addPbftBlockPeriodToBatch(period, sync_block.pbft_blk->getBlockHash(), write_batch);

  // Remove dag blocks from non finalized column in db and add dag_block_period in DB
  uint64_t block_pos = 0;
  for (auto const& block : sync_block.dag_blocks) {
    removeDagBlockBatch(write_batch, block.getHash());
    addDagBlockPeriodToBatch(block.getHash(), period, block_pos, write_batch);
    block_pos++;
  }

  // Remove transactions from non finalized column in db and add dag_block_period in DB
  uint32_t trx_pos = 0;
  for (auto const& trx : sync_block.transactions) {
    removeTransactionToBatch(trx.getHash(), write_batch);
    addTransactionPeriodToBatch(write_batch, trx.getHash(), sync_block.pbft_blk->getPeriod(), trx_pos);
    trx_pos++;
  }

  insert(write_batch, Columns::period_data, toSlice(period), toSlice(sync_block.rlp()));
}

dev::bytes DbStorage::getPeriodDataRaw(uint64_t period) const {
  return asBytes(lookup(toSlice(period), Columns::period_data));
}

void DbStorage::saveTransaction(Transaction const& trx) {
  insert(Columns::transactions, toSlice(trx.getHash().asBytes()), toSlice(trx.rlp()));
}

void DbStorage::saveTransactionPeriod(trx_hash_t const& trx_hash, uint32_t period, uint32_t position) {
  dev::RLPStream s;
  s.appendList(2);
  s << period;
  s << position;
  insert(Columns::trx_period, toSlice(trx_hash.asBytes()), toSlice(s.out()));
}

void DbStorage::addTransactionPeriodToBatch(Batch& write_batch, trx_hash_t const& trx, uint32_t period,
                                            uint32_t position) {
  dev::RLPStream s;
  s.appendList(2);
  s << period;
  s << position;
  insert(write_batch, Columns::trx_period, toSlice(trx.asBytes()), toSlice(s.out()));
}

std::optional<std::pair<uint32_t, uint32_t>> DbStorage::getTransactionPeriod(trx_hash_t const& hash) {
  auto data = lookup(toSlice(hash.asBytes()), Columns::trx_period);
  if (!data.empty()) {
    std::pair<uint32_t, uint32_t> res;
    dev::RLP const rlp(data);
    auto it = rlp.begin();
    res.first = (*it++).toInt<uint32_t>();
    res.second = (*it++).toInt<uint32_t>();
    return res;
  }
  return std::nullopt;
}

std::vector<bool> DbStorage::transactionsFinalized(std::vector<trx_hash_t> const& trx_hashes) {
  std::vector<bool> result;
  result.reserve(trx_hashes.size());

  DbStorage::MultiGetQuery db_query(shared_from_this(), trx_hashes.size());
  db_query.append(DbStorage::Columns::trx_period, trx_hashes);
  auto db_trxs_period = db_query.execute();
  for (size_t idx = 0; idx < db_trxs_period.size(); idx++) {
    auto& trx_raw_period = db_trxs_period[idx];
    result.push_back(!trx_raw_period.empty());
  }

  return result;
}

std::unordered_map<trx_hash_t, uint32_t> DbStorage::getAllTransactionPeriod() {
  std::unordered_map<trx_hash_t, uint32_t> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::trx_period)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    auto data = asBytes(i->value().ToString());
    dev::RLP const rlp(data);
    auto it = rlp.begin();
    res[trx_hash_t(asBytes(i->key().ToString()))] = (*it++).toInt<uint32_t>();
  }
  return res;
}

std::optional<PbftBlock> DbStorage::getPbftBlock(uint64_t period) const {
  auto period_data = getPeriodDataRaw(period);
  // DB is corrupted if status point to missing or incorrect transaction
  if (period_data.size() > 0) {
    auto period_data_rlp = dev::RLP(period_data);
    return std::optional<PbftBlock>(period_data_rlp[PBFT_BLOCK_POS_IN_PERIOD_DATA]);
  }
  return {};
}

blk_hash_t DbStorage::getPeriodBlockHash(uint64_t period) const {
  const auto& blk = getPbftBlock(period);
  if (blk.has_value()) {
    return blk->getBlockHash();
  }
  return {};
}

std::shared_ptr<Transaction> DbStorage::getTransaction(trx_hash_t const& hash) {
  auto data = asBytes(lookup(toSlice(hash.asBytes()), Columns::transactions));
  if (data.size() > 0) {
    return std::make_shared<Transaction>(data);
  }
  auto res = getTransactionPeriod(hash);
  if (res) {
    auto period_data = getPeriodDataRaw(res->first);
    if (period_data.size() > 0) {
      auto period_data_rlp = dev::RLP(period_data);
      auto transaction_data = period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA];
      return std::make_shared<Transaction>(transaction_data[res->second]);
    }
  }
  return nullptr;
}

std::optional<std::vector<Transaction>> DbStorage::getPeriodTransactions(uint64_t period) const {
  const auto period_data = getPeriodDataRaw(period);
  if (!period_data.size()) {
    return std::nullopt;
  }

  auto period_data_rlp = dev::RLP(period_data);

  std::vector<Transaction> ret(period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA].size());
  for (const auto transaction_data : period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA]) {
    ret.emplace_back(transaction_data);
  }
  return {ret};
}

void DbStorage::addTransactionToBatch(Transaction const& trx, Batch& write_batch) {
  insert(write_batch, DbStorage::Columns::transactions, toSlice(trx.getHash().asBytes()), toSlice(trx.rlp()));
}

void DbStorage::removeTransactionToBatch(trx_hash_t const& trx, Batch& write_batch) {
  remove(write_batch, Columns::transactions, toSlice(trx));
}

bool DbStorage::transactionInDb(trx_hash_t const& hash) {
  return exist(toSlice(hash.asBytes()), Columns::transactions) || exist(toSlice(hash.asBytes()), Columns::trx_period);
}

bool DbStorage::transactionFinalized(trx_hash_t const& hash) {
  return exist(toSlice(hash.asBytes()), Columns::trx_period);
}

std::vector<bool> DbStorage::transactionsInDb(std::vector<trx_hash_t> const& trx_hashes) {
  std::vector<bool> result(trx_hashes.size(), false);

  DbStorage::MultiGetQuery db_query(shared_from_this(), trx_hashes.size());
  db_query.append(DbStorage::Columns::transactions, trx_hashes);
  auto db_trxs_statuses = db_query.execute();
  for (size_t idx = 0; idx < db_trxs_statuses.size(); idx++) {
    auto& trx_raw_status = db_trxs_statuses[idx];
    result[idx] = !trx_raw_status.empty();
  }

  db_query.append(DbStorage::Columns::trx_period, trx_hashes);
  db_trxs_statuses = db_query.execute();
  for (size_t idx = 0; idx < db_trxs_statuses.size(); idx++) {
    auto& trx_raw_status = db_trxs_statuses[idx];
    result[idx] = result[idx] || (!trx_raw_status.empty());
  }

  return result;
}

uint64_t DbStorage::getStatusField(StatusDbField const& field) {
  auto status = lookup(toSlice((uint8_t)field), Columns::status);
  if (!status.empty()) {
    uint64_t value;
    memcpy(&value, status.data(), sizeof(uint64_t));
    return value;
  }

  return 0;
}

void DbStorage::saveStatusField(StatusDbField const& field, uint64_t value) {
  insert(Columns::status, toSlice((uint8_t)field), toSlice(value));
}

void DbStorage::addStatusFieldToBatch(StatusDbField const& field, uint64_t value, Batch& write_batch) {
  insert(write_batch, DbStorage::Columns::status, toSlice((uint8_t)field), toSlice(value));
}

// PBFT
uint64_t DbStorage::getPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus field) {
  auto status = lookup(toSlice(field), Columns::pbft_mgr_previous_round_status);
  if (!status.empty()) {
    size_t value;
    memcpy(&value, status.data(), sizeof(uint64_t));
    return value;
  }

  return 0;
}

void DbStorage::savePbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus field, uint64_t value) {
  insert(Columns::pbft_mgr_previous_round_status, toSlice(field), toSlice(value));
}

void DbStorage::addPbftMgrPreviousRoundStatus(PbftMgrPreviousRoundStatus field, uint64_t value, Batch& write_batch) {
  insert(write_batch, Columns::pbft_mgr_previous_round_status, toSlice(field), toSlice(value));
}

uint64_t DbStorage::getPbftMgrField(PbftMgrRoundStep field) {
  auto status = lookup(toSlice(field), Columns::pbft_mgr_round_step);
  if (!status.empty()) {
    uint64_t value;
    memcpy(&value, status.data(), sizeof(uint64_t));
    return value;
  }

  return 1;
}

void DbStorage::savePbftMgrField(PbftMgrRoundStep field, uint64_t value) {
  insert(Columns::pbft_mgr_round_step, toSlice(field), toSlice(value));
}

void DbStorage::addPbftMgrFieldToBatch(PbftMgrRoundStep field, uint64_t value, Batch& write_batch) {
  insert(write_batch, DbStorage::Columns::pbft_mgr_round_step, toSlice(field), toSlice(value));
}

size_t DbStorage::getPbft2TPlus1(uint64_t round) {
  auto status = lookup(toSlice(round), Columns::pbft_round_2t_plus_1);

  if (!status.empty()) {
    size_t value;
    memcpy(&value, status.data(), sizeof(size_t));
    return value;
  }

  return 0;
}

// Only for test
void DbStorage::savePbft2TPlus1(uint64_t pbft_round, size_t pbft_2t_plus_1) {
  insert(Columns::pbft_round_2t_plus_1, toSlice(pbft_round), toSlice(pbft_2t_plus_1));
}

void DbStorage::addPbft2TPlus1ToBatch(uint64_t pbft_round, size_t pbft_2t_plus_1, Batch& write_batch) {
  insert(write_batch, DbStorage::Columns::pbft_round_2t_plus_1, toSlice(pbft_round), toSlice(pbft_2t_plus_1));
}

bool DbStorage::getPbftMgrStatus(PbftMgrStatus field) {
  auto status = lookup(toSlice(field), Columns::pbft_mgr_status);
  if (!status.empty()) {
    return *(bool*)&status[0];
  }
  return false;
}

void DbStorage::savePbftMgrStatus(PbftMgrStatus field, bool const& value) {
  insert(Columns::pbft_mgr_status, toSlice(field), toSlice(value));
}

void DbStorage::addPbftMgrStatusToBatch(PbftMgrStatus field, bool const& value, Batch& write_batch) {
  insert(write_batch, DbStorage::Columns::pbft_mgr_status, toSlice(field), toSlice(value));
}

std::shared_ptr<blk_hash_t> DbStorage::getPbftMgrVotedValue(PbftMgrVotedValue field) {
  auto hash = asBytes(lookup(toSlice(field), Columns::pbft_mgr_voted_value));
  if (hash.size() > 0) {
    return std::make_shared<blk_hash_t>(hash);
  }
  return nullptr;
}

void DbStorage::savePbftMgrVotedValue(PbftMgrVotedValue field, blk_hash_t const& value) {
  insert(Columns::pbft_mgr_voted_value, toSlice(field), toSlice(value.asBytes()));
}

void DbStorage::addPbftMgrVotedValueToBatch(PbftMgrVotedValue field, blk_hash_t const& value, Batch& write_batch) {
  insert(write_batch, Columns::pbft_mgr_voted_value, toSlice(field), toSlice(value.asBytes()));
}

std::shared_ptr<PbftBlock> DbStorage::getPbftCertVotedBlock(blk_hash_t const& block_hash) {
  auto block = lookup(toSlice(block_hash.asBytes()), Columns::pbft_cert_voted_block);
  if (!block.empty()) {
    return std::make_shared<PbftBlock>(dev::RLP(block));
  }
  return nullptr;
}

// Only for test
void DbStorage::savePbftCertVotedBlock(PbftBlock const& pbft_block) {
  insert(Columns::pbft_cert_voted_block, toSlice(pbft_block.getBlockHash().asBytes()), toSlice(pbft_block.rlp(true)));
}

void DbStorage::addPbftCertVotedBlockToBatch(PbftBlock const& pbft_block, Batch& write_batch) {
  insert(write_batch, Columns::pbft_cert_voted_block, toSlice(pbft_block.getBlockHash().asBytes()),
         toSlice(pbft_block.rlp(true)));
}

std::optional<PbftBlock> DbStorage::getPbftBlock(blk_hash_t const& hash) {
  auto res = getPeriodFromPbftHash(hash);
  if (res.first) {
    return getPbftBlock(res.second);
  }
  return {};
}

bool DbStorage::pbftBlockInDb(blk_hash_t const& hash) {
  return exist(toSlice(hash.asBytes()), Columns::pbft_block_period);
}

string DbStorage::getPbftHead(blk_hash_t const& hash) { return lookup(toSlice(hash.asBytes()), Columns::pbft_head); }

void DbStorage::savePbftHead(blk_hash_t const& hash, string const& pbft_chain_head_str) {
  insert(Columns::pbft_head, toSlice(hash.asBytes()), pbft_chain_head_str);
}

void DbStorage::addPbftHeadToBatch(taraxa::blk_hash_t const& head_hash, std::string const& head_str,
                                   Batch& write_batch) {
  insert(write_batch, Columns::pbft_head, toSlice(head_hash.asBytes()), head_str);
}

std::vector<std::shared_ptr<Vote>> DbStorage::getVerifiedVotes() {
  std::vector<std::shared_ptr<Vote>> votes;

  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::verified_votes)));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    votes.emplace_back(std::make_shared<Vote>(asBytes(it->value().ToString())));
  }

  return votes;
}

// Only for test
std::shared_ptr<Vote> DbStorage::getVerifiedVote(vote_hash_t const& vote_hash) {
  auto vote = asBytes(lookup(toSlice(vote_hash.asBytes()), Columns::verified_votes));
  if (!vote.empty()) {
    return std::make_shared<Vote>(dev::RLP(vote));
  }
  return nullptr;
}

void DbStorage::saveVerifiedVote(std::shared_ptr<Vote> const& vote) {
  insert(Columns::verified_votes, toSlice(vote->getHash().asBytes()), toSlice(vote->rlp(true, true)));
}

void DbStorage::addVerifiedVoteToBatch(std::shared_ptr<Vote> const& vote, Batch& write_batch) {
  insert(write_batch, Columns::verified_votes, toSlice(vote->getHash().asBytes()), toSlice(vote->rlp(true, true)));
}

void DbStorage::removeVerifiedVoteToBatch(vote_hash_t const& vote_hash, Batch& write_batch) {
  remove(write_batch, Columns::verified_votes, toSlice(vote_hash.asBytes()));
}

std::vector<std::shared_ptr<Vote>> DbStorage::getSoftVotes(uint64_t pbft_round) {
  std::vector<std::shared_ptr<Vote>> soft_votes;
  auto soft_votes_raw = asBytes(lookup(toSlice(pbft_round), Columns::soft_votes));
  auto soft_votes_rlp = dev::RLP(soft_votes_raw);
  soft_votes.reserve(soft_votes_rlp.size());

  for (auto const soft_vote : soft_votes_rlp) {
    soft_votes.emplace_back(std::make_shared<Vote>(soft_vote));
  }

  return soft_votes;
}

// Only for test
void DbStorage::saveSoftVotes(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& soft_votes) {
  dev::RLPStream s(soft_votes.size());
  for (auto const& v : soft_votes) {
    s.appendRaw(v->rlp(true, true));
  }
  insert(Columns::soft_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::addSoftVotesToBatch(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& soft_votes,
                                    Batch& write_batch) {
  dev::RLPStream s(soft_votes.size());
  for (auto const& v : soft_votes) {
    s.appendRaw(v->rlp(true, true));
  }
  insert(write_batch, Columns::soft_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::removeSoftVotesToBatch(uint64_t pbft_round, Batch& write_batch) {
  remove(write_batch, Columns::soft_votes, toSlice(pbft_round));
}

std::vector<std::shared_ptr<Vote>> DbStorage::getCertVotes(uint64_t period) {
  std::vector<std::shared_ptr<Vote>> cert_votes;
  auto period_data = getPeriodDataRaw(period);
  if (period_data.size() > 0) {
    auto period_data_rlp = dev::RLP(period_data);
    auto cert_votes_data = period_data_rlp[CERT_VOTES_POS_IN_PERIOD_DATA];
    cert_votes.reserve(cert_votes_data.size());
    for (auto const vote : cert_votes_data) {
      cert_votes.emplace_back(std::make_shared<Vote>(vote));
    }
  }

  return cert_votes;
}

std::vector<std::shared_ptr<Vote>> DbStorage::getNextVotes(uint64_t pbft_round) {
  std::vector<std::shared_ptr<Vote>> next_votes;
  auto next_votes_raw = asBytes(lookup(toSlice(pbft_round), Columns::next_votes));
  auto next_votes_rlp = dev::RLP(next_votes_raw);
  next_votes.reserve(next_votes_rlp.size());

  for (auto const next_vote : next_votes_rlp) {
    next_votes.emplace_back(std::make_shared<Vote>(next_vote));
  }

  return next_votes;
}

void DbStorage::saveNextVotes(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& next_votes) {
  dev::RLPStream s(next_votes.size());
  for (auto const& v : next_votes) {
    s.appendRaw(v->rlp(true, true));
  }
  insert(Columns::next_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::addNextVotesToBatch(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& next_votes,
                                    Batch& write_batch) {
  dev::RLPStream s(next_votes.size());
  for (auto const& v : next_votes) {
    s.appendRaw(v->rlp(true, true));
  }
  insert(write_batch, Columns::next_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::removeNextVotesToBatch(uint64_t pbft_round, Batch& write_batch) {
  remove(write_batch, Columns::next_votes, toSlice(pbft_round));
}

void DbStorage::addPbftBlockPeriodToBatch(uint64_t period, taraxa::blk_hash_t const& pbft_block_hash,
                                          Batch& write_batch) {
  insert(write_batch, Columns::pbft_block_period, toSlice(pbft_block_hash.asBytes()), toSlice(period));
}

std::pair<bool, uint64_t> DbStorage::getPeriodFromPbftHash(taraxa::blk_hash_t const& pbft_block_hash) {
  auto data = lookup(toSlice(pbft_block_hash.asBytes()), Columns::pbft_block_period);

  if (!data.empty()) {
    uint64_t value;
    memcpy(&value, data.data(), sizeof(uint64_t));
    return {true, value};
  }

  return {false, 0};
}

std::shared_ptr<std::pair<uint32_t, uint32_t>> DbStorage::getDagBlockPeriod(blk_hash_t const& hash) {
  auto data = asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_block_period));
  if (data.size() > 0) {
    dev::RLP rlp(data);
    auto it = rlp.begin();
    auto period = (*it++).toInt<uint32_t>();
    auto position = (*it++).toInt<uint32_t>();

    return std::make_shared<std::pair<uint32_t, uint32_t>>(period, position);
  }
  return nullptr;
}

void DbStorage::addDagBlockPeriodToBatch(blk_hash_t const& hash, uint32_t period, uint32_t position,
                                         Batch& write_batch) {
  dev::RLPStream s;
  s.appendList(2);
  s << period;
  s << position;
  insert(write_batch, Columns::dag_block_period, toSlice(hash.asBytes()), toSlice(s.invalidate()));
}

std::vector<blk_hash_t> DbStorage::getFinalizedDagBlockHashesByPeriod(uint32_t period) {
  std::vector<blk_hash_t> ret;
  if (auto period_data = getPeriodDataRaw(period); period_data.size() > 0) {
    auto dag_blocks_data = dev::RLP(period_data)[DAG_BLOCKS_POS_IN_PERIOD_DATA];
    ret.reserve(dag_blocks_data.size());
    std::transform(dag_blocks_data.begin(), dag_blocks_data.end(), std::back_inserter(ret),
                   [](const auto& dag_block) { return DagBlock(dag_block).getHash(); });
  }

  return ret;
}

std::optional<uint64_t> DbStorage::getProposalPeriodForDagLevel(uint64_t level) {
  auto it =
      std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::proposal_period_levels_map)));
  it->Seek(toSlice(level));

  // this means that no sortition_params_change in database. It could be met in the tests
  if (!it->Valid()) {
    return {};
  }

  uint64_t value;
  memcpy(&value, it->value().data(), sizeof(uint64_t));
  return value;
}

void DbStorage::saveProposalPeriodDagLevelsMap(uint64_t level, uint64_t period) {
  insert(Columns::proposal_period_levels_map, toSlice(level), toSlice(period));
}

void DbStorage::addProposalPeriodDagLevelsMapToBatch(uint64_t level, uint64_t period, Batch& write_batch) {
  insert(write_batch, Columns::proposal_period_levels_map, toSlice(level), toSlice(period));
}

uint64_t DbStorage::getColumnSize(Column const& col) const {
  rocksdb::ColumnFamilyMetaData data;
  db_->GetColumnFamilyMetaData(handle(col), &data);
  return data.size;
}

void DbStorage::forEach(Column const& col, OnEntry const& f) {
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(col)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    f(i->key(), i->value());
  }
}

DbStorage::MultiGetQuery::MultiGetQuery(std::shared_ptr<DbStorage> const& db, uint capacity) : db_(db) {
  if (capacity) {
    cfs_.reserve(capacity);
    keys_.reserve(capacity);
    str_pool_.reserve(capacity);
  }
}

uint DbStorage::MultiGetQuery::size() { return keys_.size(); }

std::vector<std::string> DbStorage::MultiGetQuery::execute(bool and_reset) {
  auto _size = size();
  if (_size == 0) {
    return {};
  }
  std::vector<std::string> ret(_size);
  uint i = 0;
  for (auto const& s : db_->db_->MultiGet(db_->read_options_, cfs_, keys_, &ret)) {
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
