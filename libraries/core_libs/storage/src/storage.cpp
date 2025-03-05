#include "storage/storage.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <regex>

#include "config/version.hpp"
#include "dag/dag_block_bundle_rlp.hpp"
#include "dag/sortition_params_manager.hpp"
#include "final_chain/data.hpp"
#include "pillar_chain/pillar_block.hpp"
#include "rocksdb/utilities/checkpoint.h"
#include "transaction/system_transaction.hpp"
#include "vote/pbft_vote.hpp"
#include "vote/votes_bundle_rlp.hpp"

namespace taraxa {
namespace fs = std::filesystem;

static constexpr uint16_t PBFT_BLOCK_POS_IN_PERIOD_DATA = 0;
static constexpr uint16_t CERT_VOTES_POS_IN_PERIOD_DATA = 1;
static constexpr uint16_t DAG_BLOCKS_POS_IN_PERIOD_DATA = 2;
static constexpr uint16_t TRANSACTIONS_POS_IN_PERIOD_DATA = 3;
static constexpr uint16_t PILLAR_VOTES_POS_IN_PERIOD_DATA = 4;
static constexpr uint16_t PREV_BLOCK_HASH_POS_IN_PBFT_BLOCK = 0;

DbStorage::DbStorage(const fs::path& path, uint32_t db_snapshot_each_n_pbft_block, uint32_t max_open_files,
                     uint32_t db_max_snapshots, PbftPeriod db_revert_to_period, addr_t node_addr, bool rebuild)
    : path_(path),
      handles_(Columns::all.size()),
      kDbSnapshotsEachNblock(db_snapshot_each_n_pbft_block),
      kDbSnapshotsMaxCount(db_max_snapshots) {
  db_path_ = (path / kDbDir);
  state_db_path_ = (path / kStateDbDir);
  async_write_.sync = false;
  sync_write_.sync = true;

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
  LOG_OBJECTS_CREATE("DBS");

  fs::create_directories(db_path_);
  removeTempFiles();

  rocksdb::Options options;
  options.create_missing_column_families = true;
  options.create_if_missing = true;
  options.compression = rocksdb::CompressionType::kLZ4Compression;
  // DON'T CHANGE THIS VALUE, IT WILL BREAK THE DB MEMORY USAGE
  options.max_total_wal_size = 10 << 20;                          // 10MB
  options.db_write_buffer_size = size_t(2) * 1024 * 1024 * 1024;  // 2GB
  ///////////////////////////////////////////////
  // This option is related to memory consumption
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

  rebuildColumns(options);

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
  db_->EnableFileDeletions();
  dag_blocks_count_.store(getStatusField(StatusDbField::DagBlkCount));
  dag_edge_count_.store(getStatusField(StatusDbField::DagEdgeCount));

  kMajorVersion_ = getStatusField(StatusDbField::DbMajorVersion);
  uint32_t minor_version = getStatusField(StatusDbField::DbMinorVersion);
  if (kMajorVersion_ != 0 && kMajorVersion_ != TARAXA_DB_MAJOR_VERSION) {
    major_version_changed_ = true;
  } else if (minor_version != TARAXA_DB_MINOR_VERSION) {
    minor_version_changed_ = true;
  }
}

void DbStorage::removeTempFiles() const {
  const std::regex filePattern("LOG\\.old\\.\\d+");
  removeFilesWithPattern(db_path_, filePattern);
  removeFilesWithPattern(state_db_path_, filePattern);
  deleteTmpDirectories(path_);
}

void DbStorage::removeFilesWithPattern(const std::string& directory, const std::regex& pattern) const {
  if (!std::filesystem::exists(directory)) {
    return;
  }

  try {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
      const std::string& filename = entry.path().filename().string();
      if (std::regex_match(filename, pattern)) {
        std::filesystem::remove(entry.path());
        LOG(log_dg_) << "Removed file: " << filename << std::endl;
      }
    }
  } catch (const std::filesystem::filesystem_error& e) {
    LOG(log_dg_) << "Error accessing directory: " << e.what() << std::endl;
  }
}

void DbStorage::deleteTmpDirectories(const std::string& path) const {
  try {
    for (const auto& entry : fs::directory_iterator(path)) {
      if (entry.is_directory()) {
        std::string dirName = entry.path().filename().string();
        if (dirName.size() >= 4 && dirName.substr(dirName.size() - 4) == ".tmp") {
          fs::remove_all(entry.path());
          LOG(log_dg_) << "Deleted: " << entry.path() << std::endl;
        }
      }
    }
  } catch (const fs::filesystem_error& e) {
    LOG(log_er_) << "Error: " << e.what() << std::endl;
  }
}

void DbStorage::updateDbVersions() {
  saveStatusField(StatusDbField::DbMajorVersion, TARAXA_DB_MAJOR_VERSION);
  saveStatusField(StatusDbField::DbMinorVersion, TARAXA_DB_MINOR_VERSION);
  kMajorVersion_ = TARAXA_DB_MAJOR_VERSION;
}

std::unique_ptr<rocksdb::ColumnFamilyHandle> DbStorage::copyColumn(rocksdb::ColumnFamilyHandle* orig_column,
                                                                   const std::string& new_col_name, bool move_data) {
  auto it = getColumnIterator(orig_column);
  // No data to be copied
  if (it->SeekToFirst(); !it->Valid()) {
    return nullptr;
  }

  rocksdb::Checkpoint* checkpoint_raw = nullptr;
  auto status = rocksdb::Checkpoint::Create(db_.get(), &checkpoint_raw);
  std::unique_ptr<rocksdb::Checkpoint> checkpoint(checkpoint_raw);
  checkStatus(status);

  const fs::path export_dir = path() / "migrations" / new_col_name;
  fs::create_directory(export_dir.parent_path());

  // Export dir should not exist before exporting the column family
  fs::remove_all(export_dir);

  rocksdb::ExportImportFilesMetaData* metadata_raw = nullptr;
  status = checkpoint->ExportColumnFamily(orig_column, export_dir, &metadata_raw);
  std::unique_ptr<rocksdb::ExportImportFilesMetaData> metadata(metadata_raw);
  checkStatus(status);

  const rocksdb::Comparator* comparator = orig_column->GetComparator();
  auto options = rocksdb::ColumnFamilyOptions();
  if (comparator != nullptr) {
    options.comparator = comparator;
  }

  rocksdb::ImportColumnFamilyOptions import_options;
  import_options.move_files = move_data;

  rocksdb::ColumnFamilyHandle* copied_column_raw = nullptr;
  status = db_->CreateColumnFamilyWithImport(options, new_col_name, import_options, *metadata, &copied_column_raw);
  std::unique_ptr<rocksdb::ColumnFamilyHandle> copied_column(copied_column_raw);
  checkStatus(status);

  // Remove export dir after successful import
  fs::remove_all(export_dir);

  return copied_column;
}

void DbStorage::replaceColumn(const Column& to_be_replaced_col,
                              std::unique_ptr<rocksdb::ColumnFamilyHandle>&& replacing_col) {
  checkStatus(db_->DropColumnFamily(handle(to_be_replaced_col)));
  db_->DestroyColumnFamilyHandle(handle(to_be_replaced_col));

  std::unique_ptr<rocksdb::ColumnFamilyHandle> replaced_col =
      copyColumn(replacing_col.get(), to_be_replaced_col.name(), true);

  if (!replaced_col) {
    LOG(log_er_) << "Unable to replace column " << to_be_replaced_col.name() << " by " << replacing_col->GetName()
                 << " due to failed copy";
    return;
  }

  handles_[to_be_replaced_col.ordinal_] = replaced_col.release();
  checkStatus(db_->DropColumnFamily(replacing_col.get()));
}

void DbStorage::deleteColumnData(const Column& c) {
  checkStatus(db_->DropColumnFamily(handle(c)));
  db_->DestroyColumnFamilyHandle(handle(c));

  auto options = rocksdb::ColumnFamilyOptions();
  if (c.comparator_) {
    options.comparator = c.comparator_;
  }
  checkStatus(db_->CreateColumnFamily(options, c.name(), &handles_[c.ordinal_]));
}

void DbStorage::rebuildColumns(const rocksdb::Options& options) {
  std::unique_ptr<rocksdb::DB> db;
  std::vector<std::string> column_families;
  rocksdb::DB::ListColumnFamilies(options, db_path_.string(), &column_families);
  if (column_families.empty()) {
    LOG(log_wr_) << "DB isn't initialized in rebuildColumns. Skip it";
    return;
  }

  std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
  descriptors.reserve(column_families.size());
  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  handles.reserve(column_families.size());
  std::transform(column_families.begin(), column_families.end(), std::back_inserter(descriptors), [](const auto& name) {
    const auto it = std::find_if(Columns::all.begin(), Columns::all.end(), [&name](const Column& col) {
      // "-copy" is there, so we will removed unsuccessful migrations
      return col.name() == name || col.name() + "-copy" == name;
    });
    auto options = rocksdb::ColumnFamilyOptions();
    if (it != Columns::all.end() && it->comparator_) options.comparator = it->comparator_;
    return rocksdb::ColumnFamilyDescriptor(name, options);
  });
  rocksdb::DB* db_ptr = nullptr;
  checkStatus(rocksdb::DB::Open(options, db_path_.string(), descriptors, &handles, &db_ptr));
  assert(db_ptr);
  db.reset(db_ptr);
  for (size_t i = 0; i < handles.size(); ++i) {
    const auto it = std::find_if(Columns::all.begin(), Columns::all.end(),
                                 [&handles, i](const Column& col) { return col.name() == handles[i]->GetName(); });
    if (it == Columns::all.end()) {
      // All the data from dag_blocks_index is moved to dag_blocks_level
      if (handles[i]->GetName() == "dag_blocks_index") {
        rocksdb::ColumnFamilyHandle* handle_dag_blocks_level;

        auto options = rocksdb::ColumnFamilyOptions();
        options.comparator = getIntComparator<uint64_t>();
        checkStatus(db->CreateColumnFamily(options, Columns::dag_blocks_level.name(), &handle_dag_blocks_level));

        auto it_dag_level = std::unique_ptr<rocksdb::Iterator>(db->NewIterator(read_options_, handles[i]));
        it_dag_level->SeekToFirst();

        while (it_dag_level->Valid()) {
          checkStatus(db->Put(async_write_, handle_dag_blocks_level, toSlice(it_dag_level->key()),
                              toSlice(it_dag_level->value())));
          it_dag_level->Next();
        }
      }
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
    PbftPeriod dir_period = 0;

    try {
      // Check for db or state_db prefix
      if (boost::starts_with(fileName, kDbDir) && fileName.size() > kDbDir.size()) {
        dir_period = stoi(fileName.substr(kDbDir.size()));
      } else if (boost::starts_with(fileName, kStateDbDir) && fileName.size() > kStateDbDir.size()) {
        dir_period = stoi(fileName.substr(kStateDbDir.size()));
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

bool DbStorage::createSnapshot(PbftPeriod period) {
  // Only creates snapshot each kDbSnapshotsEachNblock periods
  if (!snapshots_enabled_ || kDbSnapshotsEachNblock <= 0 || period % kDbSnapshotsEachNblock != 0 ||
      snapshots_.find(period) != snapshots_.end()) {
    return false;
  }

  LOG(log_nf_) << "Creating DB snapshot on period: " << period;

  // Create rocksdb checkpoint/snapshot
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

  // Delete any snapshot over kDbSnapshotsMaxCount
  if (kDbSnapshotsMaxCount && snapshots_.size() > kDbSnapshotsMaxCount) {
    while (snapshots_.size() > kDbSnapshotsMaxCount) {
      auto snapshot = snapshots_.begin();
      deleteSnapshot(*snapshot);
      snapshots_.erase(snapshot);
    }
  }
  return true;
}

void DbStorage::recoverToPeriod(PbftPeriod period) {
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

void DbStorage::deleteSnapshot(PbftPeriod period) {
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

void DbStorage::disableSnapshots() { snapshots_enabled_ = false; }

void DbStorage::enableSnapshots() { snapshots_enabled_ = true; }

void DbStorage::setGenesisHash(const h256& genesis_hash) {
  if (!exist(0, Columns::genesis)) {
    insert(Columns::genesis, 0, genesis_hash);
  }
}

std::optional<h256> DbStorage::getGenesisHash() {
  auto hash = asBytes(lookup(0, Columns::genesis));
  if (hash.size() > 0) {
    return h256(hash);
  }
  return {};
}

DbStorage::~DbStorage() {
  for (auto cf : handles_) {
    if (cf->GetName() != "default") {
      checkStatus(db_->DestroyColumnFamilyHandle(cf));
    }
  }
  checkStatus(db_->Close());
}

uint32_t DbStorage::getMajorVersion() const { return kMajorVersion_; }

std::unique_ptr<rocksdb::Iterator> DbStorage::getColumnIterator(const Column& c) {
  return std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(c)));
}

std::unique_ptr<rocksdb::Iterator> DbStorage::getColumnIterator(rocksdb::ColumnFamilyHandle* c) {
  return std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, c));
}

void DbStorage::checkStatus(rocksdb::Status const& status) {
  if (status.ok()) return;
  throw DbException(std::string("Db error. Status code: ") + std::to_string(status.code()) +
                    " SubCode: " + std::to_string(status.subcode()) + " Message:" + status.ToString());
}

Batch DbStorage::createWriteBatch() { return Batch(); }

void DbStorage::commitWriteBatch(Batch& write_batch, rocksdb::WriteOptions const& opts) {
  auto status = db_->Write(opts, write_batch.GetWriteBatch());
  checkStatus(status);
  write_batch.Clear();
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
      return decodeDAGBlockBundleRlp(data->second, dag_blocks_data);
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
  auto data = asBytes(lookup(toSlice(level), Columns::dag_blocks_level));
  dev::RLP rlp(data);
  return rlp.toSet<blk_hash_t>();
}

level_t DbStorage::getLastBlocksLevel() const {
  level_t level = 0;
  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::dag_blocks_level)));
  it->SeekToLast();
  if (it->Valid()) {
    memcpy(&level, it->key().data(), sizeof(level_t));
  }
  return level;
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

std::map<level_t, std::vector<std::shared_ptr<DagBlock>>> DbStorage::getNonfinalizedDagBlocks() {
  std::map<level_t, std::vector<std::shared_ptr<DagBlock>>> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::dag_blocks)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    auto block = std::make_shared<DagBlock>(asBytes(i->value().ToString()));
    res[block->getLevel()].emplace_back(std::move(block));
  }
  return res;
}

SharedTransactions DbStorage::getAllNonfinalizedTransactions() {
  SharedTransactions res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::transactions)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    res.emplace_back(std::make_shared<Transaction>(asBytes(i->value().ToString())));
  }
  return res;
}

void DbStorage::removeDagBlockBatch(Batch& write_batch, blk_hash_t const& hash) {
  remove(write_batch, Columns::dag_blocks, toSlice(hash));
}

void DbStorage::removeDagBlock(blk_hash_t const& hash) { remove(Columns::dag_blocks, toSlice(hash)); }

void DbStorage::updateDagBlockCounters(std::vector<std::shared_ptr<DagBlock>> blks) {
  // Lock is needed since we are editing some fields
  std::lock_guard<std::mutex> u_lock(dag_blocks_mutex_);
  auto write_batch = createWriteBatch();
  for (auto const& blk : blks) {
    auto level = blk->getLevel();
    auto block_hashes = getBlocksByLevel(level);
    block_hashes.emplace(blk->getHash());
    dev::RLPStream blocks_stream(block_hashes.size());
    for (auto const& hash : block_hashes) {
      blocks_stream << hash;
    }
    insert(write_batch, Columns::dag_blocks_level, toSlice(level), toSlice(blocks_stream.out()));
    dag_blocks_count_.fetch_add(1);
    dag_edge_count_.fetch_add(blk->getTips().size() + 1);
  }
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagBlkCount), toSlice(dag_blocks_count_.load()));
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagEdgeCount), toSlice(dag_edge_count_.load()));
  commitWriteBatch(write_batch);
}

void DbStorage::saveDagBlock(const std::shared_ptr<DagBlock>& blk, Batch* write_batch_p) {
  // Lock is needed since we are editing some fields
  std::lock_guard<std::mutex> u_lock(dag_blocks_mutex_);
  auto write_batch_up = write_batch_p ? std::unique_ptr<Batch>() : std::make_unique<Batch>();
  auto commit = !write_batch_p;
  auto& write_batch = write_batch_p ? *write_batch_p : *write_batch_up;
  auto block_bytes = blk->rlp(true);
  auto block_hash = blk->getHash();
  insert(write_batch, Columns::dag_blocks, toSlice(block_hash.asBytes()), toSlice(block_bytes));
  auto level = blk->getLevel();
  auto block_hashes = getBlocksByLevel(level);
  block_hashes.emplace(blk->getHash());
  dev::RLPStream blocks_stream(block_hashes.size());
  for (auto const& hash : block_hashes) {
    blocks_stream << hash;
  }
  insert(write_batch, Columns::dag_blocks_level, toSlice(level), toSlice(blocks_stream.out()));

  dag_blocks_count_.fetch_add(1);
  dag_edge_count_.fetch_add(blk->getTips().size() + 1);
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagBlkCount), toSlice(dag_blocks_count_.load()));
  insert(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagEdgeCount), toSlice(dag_edge_count_.load()));
  if (commit) {
    commitWriteBatch(write_batch);
  }
}

// Sortition params
void DbStorage::saveSortitionParamsChange(PbftPeriod period, const SortitionParamsChange& params, Batch& batch) {
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

std::optional<SortitionParamsChange> DbStorage::getParamsChangeForPeriod(PbftPeriod period) {
  auto it =
      std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::sortition_params_change)));
  it->SeekForPrev(toSlice(period));

  // this means that no sortition_params_change in database. It could be met in the tests
  if (!it->Valid()) {
    return {};
  }

  return SortitionParamsChange::from_rlp(dev::RLP(it->value().ToString()));
}

void DbStorage::clearPeriodDataHistory(PbftPeriod end_period, uint64_t dag_level_to_keep,
                                       PbftPeriod last_block_number) {
  LOG(log_si_) << "Clear light node history";

  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::period_data)));
  // Find the first non-deleted period
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }

  uint64_t start_period;
  memcpy(&start_period, it->key().data(), sizeof(uint64_t));
  auto start_slice = toSlice(start_period);
  auto end_slice = toSlice(end_period);

  db_->DeleteRange(async_write_, handle(Columns::period_data), start_slice, end_slice);
  db_->DeleteRange(async_write_, handle(Columns::pillar_block), start_slice, end_slice);
  db_->CompactRange({}, handle(Columns::period_data), &start_slice, &end_slice);
  db_->CompactRange({}, handle(Columns::pillar_block), &start_slice, &end_slice);

  std::unordered_set<trx_hash_t> trxs;
  std::unordered_set<blk_hash_t> dag_blocks;
  std::unordered_set<blk_hash_t> pbft_blocks;

  const uint64_t periods_to_keep_non_block_data = 1000;
  for (uint64_t period = last_block_number - periods_to_keep_non_block_data;; period++) {
    auto period_data = getPeriodData(period);
    if (!period_data.has_value()) {
      break;
    }
    for (auto t : period_data->transactions) {
      trxs.insert(t->getHash());
    }
    for (auto d : period_data->dag_blocks) {
      dag_blocks.insert(d->getHash());
    }
    pbft_blocks.insert(period_data->pbft_blk->getBlockHash());
  }

  auto clearColumnHistory = [this]<typename T>(std::unordered_set<T>& to_keep, Column c) {
    std::map<trx_hash_t, bytes> data_to_keep;
    for (auto t : to_keep) {
      auto raw = asBytes(lookup(t, c));
      if (!raw.empty()) {
        data_to_keep[t] = raw;
      }
    }

    deleteColumnData(c);
    auto batch = createWriteBatch();
    for (auto data : data_to_keep) {
      insert(batch, c, data.first, data.second);
    }
    commitWriteBatch(batch);
    data_to_keep.clear();
  };

  clearColumnHistory(trxs, Columns::trx_period);
  clearColumnHistory(trxs, Columns::final_chain_receipt_by_trx_hash);
  clearColumnHistory(dag_blocks, Columns::dag_block_period);
  clearColumnHistory(pbft_blocks, Columns::pbft_block_period);

  it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::dag_blocks_level)));
  it->SeekToFirst();
  if (!it->Valid()) {
    return;
  }

  uint64_t start_level;
  memcpy(&start_level, it->key().data(), sizeof(uint64_t));
  start_slice = toSlice(start_level);
  end_slice = toSlice(dag_level_to_keep - 1);
  db_->DeleteRange(async_write_, handle(Columns::dag_blocks_level), start_slice, end_slice);
  db_->CompactRange({}, handle(Columns::dag_blocks_level), nullptr, nullptr);
  LOG(log_si_) << "Clear light node history completed";
}

void DbStorage::savePeriodData(const PeriodData& period_data, Batch& write_batch) {
  const auto period = period_data.pbft_blk->getPeriod();
  addPbftBlockPeriodToBatch(period, period_data.pbft_blk->getBlockHash(), write_batch);

  // Remove dag blocks from non finalized column in db and add dag_block_period in DB
  uint32_t block_pos = 0;
  for (auto const& block : period_data.dag_blocks) {
    removeDagBlockBatch(write_batch, block->getHash());
    addDagBlockPeriodToBatch(block->getHash(), period, block_pos, write_batch);
    block_pos++;
  }

  // Remove transactions from non finalized column in db and add dag_block_period in DB
  uint32_t trx_pos = 0;
  for (auto const& trx : period_data.transactions) {
    removeTransactionToBatch(trx->getHash(), write_batch);
    addTransactionLocationToBatch(write_batch, trx->getHash(), period_data.pbft_blk->getPeriod(), trx_pos);
    trx_pos++;
  }

  insert(write_batch, Columns::period_data, toSlice(period), toSlice(period_data.rlp()));
}

dev::bytes DbStorage::getPeriodDataRaw(PbftPeriod period) const {
  return asBytes(lookup(toSlice(period), Columns::period_data));
}

std::optional<PeriodData> DbStorage::getPeriodData(PbftPeriod period) const {
  auto period_data_bytes = getPeriodDataRaw(period);
  if (period_data_bytes.empty()) {
    return {};
  }

  return PeriodData{std::move(period_data_bytes)};
}

void DbStorage::savePillarBlock(const std::shared_ptr<pillar_chain::PillarBlock>& pillar_block) {
  insert(Columns::pillar_block, pillar_block->getPeriod(), pillar_block->getRlp());
}

std::shared_ptr<pillar_chain::PillarBlock> DbStorage::getPillarBlock(PbftPeriod period) const {
  const auto bytes = asBytes(lookup(period, Columns::pillar_block));
  if (bytes.empty()) {
    return {};
  }

  return std::make_shared<pillar_chain::PillarBlock>(dev::RLP(bytes));
}

std::shared_ptr<pillar_chain::PillarBlock> DbStorage::getLatestPillarBlock() const {
  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::pillar_block)));
  it->SeekToLast();
  if (!it->Valid()) {
    return {};
  }

  return std::make_shared<pillar_chain::PillarBlock>(dev::RLP(it->value().ToString()));
}

void DbStorage::saveOwnPillarBlockVote(const std::shared_ptr<PillarVote>& vote) {
  insert(Columns::current_pillar_block_own_vote, 0, util::rlp_enc(vote));
}

std::shared_ptr<PillarVote> DbStorage::getOwnPillarBlockVote() const {
  const auto bytes = asBytes(lookup(0, Columns::current_pillar_block_own_vote));
  if (bytes.empty()) {
    return nullptr;
  }

  return std::make_shared<PillarVote>(dev::RLP(bytes));
}

void DbStorage::saveCurrentPillarBlockData(const pillar_chain::CurrentPillarBlockDataDb& current_pillar_block_data) {
  insert(Columns::current_pillar_block_data, 0, util::rlp_enc(current_pillar_block_data));
}

std::optional<pillar_chain::CurrentPillarBlockDataDb> DbStorage::getCurrentPillarBlockData() const {
  const auto bytes = asBytes(lookup(0, Columns::current_pillar_block_data));
  if (bytes.empty()) {
    return {};
  }

  return util::rlp_dec<pillar_chain::CurrentPillarBlockDataDb>(dev::RLP(bytes));
}

void DbStorage::addTransactionLocationToBatch(Batch& write_batch, trx_hash_t const& trx_hash, PbftPeriod period,
                                              uint32_t position, bool is_system) {
  dev::RLPStream s;
  s.appendList(2 + is_system);
  s << period;
  s << position;
  if (is_system) {
    s << is_system;
  }
  insert(write_batch, Columns::trx_period, toSlice(trx_hash.asBytes()), toSlice(s.invalidate()));
}

std::optional<final_chain::TransactionLocation> DbStorage::getTransactionLocation(trx_hash_t const& hash) const {
  auto data = lookup(toSlice(hash.asBytes()), Columns::trx_period);
  if (!data.empty()) {
    final_chain::TransactionLocation res;
    dev::RLP const rlp(data);
    auto it = rlp.begin();
    res.period = (*it++).toInt<PbftPeriod>();
    res.position = (*it++).toInt<uint32_t>();
    if (rlp.itemCount() == 3) {
      res.is_system = (*it++).toInt<bool>();
    }
    return res;
  }
  return std::nullopt;
}

std::vector<bool> DbStorage::transactionsFinalized(std::vector<trx_hash_t> const& trx_hashes) {
  std::vector<bool> result(trx_hashes.size(), false);
  for (size_t i = 0; i < trx_hashes.size(); ++i) {
    if (exist(toSlice(trx_hashes[i].asBytes()), Columns::trx_period)) {
      result[i] = true;
    }
  }
  return result;
}

std::unordered_map<trx_hash_t, PbftPeriod> DbStorage::getAllTransactionPeriod() {
  std::unordered_map<trx_hash_t, PbftPeriod> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::trx_period)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    auto data = asBytes(i->value().ToString());
    dev::RLP const rlp(data);
    auto it = rlp.begin();
    res[trx_hash_t(asBytes(i->key().ToString()))] = (*it++).toInt<PbftPeriod>();
  }
  return res;
}

// Proposed pbft blocks
void DbStorage::saveProposedPbftBlock(const std::shared_ptr<PbftBlock>& block) {
  insert(Columns::proposed_pbft_blocks, block->getBlockHash().asBytes(), block->rlp(true));
}

void DbStorage::removeProposedPbftBlock(const blk_hash_t& block_hash, Batch& write_batch) {
  remove(write_batch, Columns::proposed_pbft_blocks, toSlice(block_hash.asBytes()));
}

std::vector<std::shared_ptr<PbftBlock>> DbStorage::getProposedPbftBlocks() {
  std::vector<std::shared_ptr<PbftBlock>> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::proposed_pbft_blocks)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    res.push_back(std::make_shared<PbftBlock>(asBytes(i->value().ToString())));
  }
  return res;
}

std::optional<PbftBlock> DbStorage::getPbftBlock(PbftPeriod period) const {
  auto period_data = getPeriodDataRaw(period);
  // DB is corrupted if status point to missing or incorrect transaction
  if (period_data.size() > 0) {
    auto period_data_rlp = dev::RLP(period_data);
    return std::optional<PbftBlock>(period_data_rlp[PBFT_BLOCK_POS_IN_PERIOD_DATA]);
  }
  return {};
}

blk_hash_t DbStorage::getPeriodBlockHash(PbftPeriod period) const {
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
  auto location = getTransactionLocation(hash);
  if (location && !location->is_system) {
    auto period_data = getPeriodDataRaw(location->period);
    if (period_data.size() > 0) {
      auto period_data_rlp = dev::RLP(period_data);
      auto transaction_data = period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA];
      return std::make_shared<Transaction>(transaction_data[location->position]);
    }
  } else {
    // get system trx from a different column
    return getSystemTransaction(hash);
  }
  return nullptr;
}

uint64_t DbStorage::getTransactionCount(PbftPeriod period) const {
  auto period_data = getPeriodDataRaw(period);
  if (period_data.size()) {
    auto period_data_rlp = dev::RLP(period_data);
    return period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA].itemCount();
  }
  return 0;
}

SharedTransactions DbStorage::getFinalizedTransactions(std::vector<trx_hash_t> const& trx_hashes) const {
  SharedTransactions trxs;
  // Map of period to position of transactions within a period
  std::map<PbftPeriod, std::set<uint32_t>> period_map;
  trxs.reserve(trx_hashes.size());
  for (auto const& tx_hash : trx_hashes) {
    auto trx_period = getTransactionLocation(tx_hash);
    if (trx_period.has_value()) {
      period_map[trx_period->period].insert(trx_period->position);
    }
  }
  for (auto it : period_map) {
    const auto period_data = getPeriodDataRaw(it.first);
    if (!period_data.size()) {
      assert(false);
    }

    auto const transactions_rlp = dev::RLP(period_data)[TRANSACTIONS_POS_IN_PERIOD_DATA];
    for (auto pos : it.second) {
      trxs.emplace_back(std::make_shared<Transaction>(transactions_rlp[pos]));
    }
  }

  return trxs;
}

void DbStorage::addSystemTransactionToBatch(Batch& write_batch, SharedTransaction trx) {
  insert(write_batch, Columns::system_transaction, toSlice(trx->getHash().asBytes()), toSlice(trx->rlp()));
}

std::shared_ptr<Transaction> DbStorage::getSystemTransaction(const trx_hash_t& hash) const {
  auto data = asBytes(lookup(toSlice(hash.asBytes()), Columns::system_transaction));
  if (data.size() > 0) {
    // construct as system transaction to have proper sender
    return std::make_shared<SystemTransaction>(data);
  }
  return nullptr;
}

void DbStorage::addPeriodSystemTransactions(Batch& write_batch, SharedTransactions trxs, PbftPeriod period) {
  std::vector<trx_hash_t> trx_hashes;
  trx_hashes.reserve(trxs.size());
  std::transform(trxs.begin(), trxs.end(), std::back_inserter(trx_hashes),
                 [](const auto& trx) { return trx->getHash(); });
  auto hashes_rlp = util::rlp_enc(trx_hashes);
  insert(write_batch, Columns::period_system_transactions, toSlice(period), toSlice(hashes_rlp));
}

std::vector<trx_hash_t> DbStorage::getPeriodSystemTransactionsHashes(PbftPeriod period) const {
  auto data = asBytes(lookup(toSlice(period), Columns::period_system_transactions));
  if (data.size() == 0) {
    return {};
  }
  return util::rlp_dec<std::vector<trx_hash_t>>(dev::RLP(data));
}

SharedTransactions DbStorage::getPeriodSystemTransactions(PbftPeriod period) const {
  auto trx_hashes = getPeriodSystemTransactionsHashes(period);
  if (trx_hashes.empty()) {
    return {};
  }

  SharedTransactions trxs;
  std::transform(trx_hashes.begin(), trx_hashes.end(), std::back_inserter(trxs),
                 [this](const auto& trx_hash) { return getSystemTransaction(trx_hash); });
  return trxs;
}

std::vector<std::shared_ptr<PbftVote>> DbStorage::getPeriodCertVotes(PbftPeriod period) const {
  auto period_data = getPeriodDataRaw(period);
  if (period_data.empty()) {
    return {};
  }

  auto period_data_rlp = dev::RLP(period_data);
  auto votes_rlp = period_data_rlp[CERT_VOTES_POS_IN_PERIOD_DATA];
  if (votes_rlp.itemCount() == 0) {
    return {};
  }
  return decodePbftVotesBundleRlp(votes_rlp);
}

std::optional<SharedTransactions> DbStorage::getPeriodTransactions(PbftPeriod period) const {
  const auto period_data = getPeriodDataRaw(period);
  if (!period_data.size()) {
    return std::nullopt;
  }

  auto period_data_rlp = dev::RLP(period_data);

  SharedTransactions ret(period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA].size());
  for (const auto transaction_data : period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA]) {
    ret.emplace_back(std::make_shared<Transaction>(transaction_data));
  }
  auto period_system_transactions = getPeriodSystemTransactions(period);
  ret.insert(ret.end(), period_system_transactions.begin(), period_system_transactions.end());
  return {ret};
}

std::vector<std::shared_ptr<PillarVote>> DbStorage::getPeriodPillarVotes(PbftPeriod period) const {
  const auto period_data = getPeriodDataRaw(period);
  if (!period_data.size()) {
    return {};
  }

  auto period_data_rlp = dev::RLP(period_data);
  // This could potentially happen if getPeriodPillarVotes is called for period that does not contain pillar votes
  if (period_data_rlp.itemCount() < PILLAR_VOTES_POS_IN_PERIOD_DATA) {
    return {};
  }

  return decodePillarVotesBundleRlp(period_data_rlp[PILLAR_VOTES_POS_IN_PERIOD_DATA]);
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
  for (size_t i = 0; i < trx_hashes.size(); ++i) {
    const auto key = trx_hashes[i].asBytes();
    if (exist(toSlice(key), Columns::transactions) || exist(toSlice(key), Columns::trx_period)) {
      result[i] = true;
    }
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

uint32_t DbStorage::getPbftMgrField(PbftMgrField field) {
  auto status = lookup(toSlice(static_cast<uint8_t>(field)), Columns::pbft_mgr_round_step);
  if (!status.empty()) {
    uint32_t value;
    memcpy(&value, status.data(), sizeof(uint32_t));
    return value;
  }

  return 1;
}

void DbStorage::savePbftMgrField(PbftMgrField field, uint32_t value) {
  insert(Columns::pbft_mgr_round_step, toSlice(static_cast<uint8_t>(field)), toSlice(value));
}

void DbStorage::addPbftMgrFieldToBatch(PbftMgrField field, uint32_t value, Batch& write_batch) {
  insert(write_batch, DbStorage::Columns::pbft_mgr_round_step, toSlice(static_cast<uint8_t>(field)), toSlice(value));
}

bool DbStorage::getPbftMgrStatus(PbftMgrStatus field) {
  auto status = lookup(toSlice(field), Columns::pbft_mgr_status);
  if (!status.empty()) {
    bool value;
    memcpy(&value, status.data(), sizeof(bool));
    return value;
  }
  return false;
}

void DbStorage::savePbftMgrStatus(PbftMgrStatus field, bool const& value) {
  insert(Columns::pbft_mgr_status, toSlice(field), toSlice(value));
}

void DbStorage::addPbftMgrStatusToBatch(PbftMgrStatus field, bool const& value, Batch& write_batch) {
  insert(write_batch, DbStorage::Columns::pbft_mgr_status, toSlice(field), toSlice(value));
}

void DbStorage::saveCertVotedBlockInRound(PbftRound round, const std::shared_ptr<PbftBlock>& block) {
  assert(block);

  dev::RLPStream s(2);
  s.append(round);
  s.appendRaw(block->rlp(true));
  insert(Columns::cert_voted_block_in_round, 0, toSlice(s.out()));
}

std::optional<std::pair<PbftRound, std::shared_ptr<PbftBlock>>> DbStorage::getCertVotedBlockInRound() const {
  auto value = asBytes(lookup(0, Columns::cert_voted_block_in_round));
  if (value.empty()) {
    return {};
  }

  auto value_rlp = dev::RLP(value);
  assert(value_rlp.itemCount() == 2);

  std::pair<PbftRound, std::shared_ptr<PbftBlock>> ret;
  ret.first = value_rlp[0].toInt<PbftRound>();
  ret.second = std::make_shared<PbftBlock>(value_rlp[1]);

  return ret;
}

void DbStorage::removeCertVotedBlockInRound(Batch& write_batch) {
  remove(write_batch, Columns::cert_voted_block_in_round, 0);
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

std::string DbStorage::getPbftHead(blk_hash_t const& hash) {
  return lookup(toSlice(hash.asBytes()), Columns::pbft_head);
}

void DbStorage::savePbftHead(blk_hash_t const& hash, std::string const& pbft_chain_head_str) {
  insert(Columns::pbft_head, toSlice(hash.asBytes()), pbft_chain_head_str);
}

void DbStorage::addPbftHeadToBatch(taraxa::blk_hash_t const& head_hash, std::string const& head_str,
                                   Batch& write_batch) {
  insert(write_batch, Columns::pbft_head, toSlice(head_hash.asBytes()), head_str);
}

void DbStorage::saveOwnVerifiedVote(const std::shared_ptr<PbftVote>& vote) {
  insert(Columns::latest_round_own_votes, vote->getHash().asBytes(), vote->rlp(true, true));
}

std::vector<std::shared_ptr<PbftVote>> DbStorage::getOwnVerifiedVotes() {
  std::vector<std::shared_ptr<PbftVote>> votes;
  auto it =
      std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::latest_round_own_votes)));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    votes.emplace_back(std::make_shared<PbftVote>(asBytes(it->value().ToString())));
  }

  return votes;
}

void DbStorage::clearOwnVerifiedVotes(Batch& write_batch,
                                      const std::vector<std::shared_ptr<PbftVote>>& own_verified_votes) {
  for (const auto& own_vote : own_verified_votes) {
    remove(write_batch, Columns::latest_round_own_votes, own_vote->getHash().asBytes());
  }
}

void DbStorage::replaceTwoTPlusOneVotes(TwoTPlusOneVotedBlockType type,
                                        const std::vector<std::shared_ptr<PbftVote>>& votes) {
  remove(Columns::latest_round_two_t_plus_one_votes, static_cast<uint8_t>(type));

  dev::RLPStream s(votes.size());
  for (const auto& vote : votes) {
    s.appendRaw(vote->rlp(true, true));
  }
  insert(Columns::latest_round_two_t_plus_one_votes, static_cast<uint8_t>(type), s.out());
}

void DbStorage::replaceTwoTPlusOneVotesToBatch(TwoTPlusOneVotedBlockType type,
                                               const std::vector<std::shared_ptr<PbftVote>>& votes,
                                               Batch& write_batch) {
  remove(write_batch, Columns::latest_round_two_t_plus_one_votes, static_cast<uint8_t>(type));

  dev::RLPStream s(votes.size());
  for (const auto& vote : votes) {
    s.appendRaw(vote->rlp(true, true));
  }
  insert(write_batch, Columns::latest_round_two_t_plus_one_votes, static_cast<uint8_t>(type), s.out());
}

std::vector<std::shared_ptr<PbftVote>> DbStorage::getAllTwoTPlusOneVotes() {
  std::vector<std::shared_ptr<PbftVote>> votes;
  auto load_db_votes = [this, &votes](TwoTPlusOneVotedBlockType type) {
    auto votes_raw = asBytes(lookup(static_cast<uint8_t>(type), Columns::latest_round_two_t_plus_one_votes));
    auto votes_rlp = dev::RLP(votes_raw);
    votes.reserve(votes.size() + votes_rlp.size());

    for (const auto vote : votes_rlp) {
      votes.emplace_back(std::make_shared<PbftVote>(vote));
    }
  };

  load_db_votes(TwoTPlusOneVotedBlockType::SoftVotedBlock);
  load_db_votes(TwoTPlusOneVotedBlockType::CertVotedBlock);
  load_db_votes(TwoTPlusOneVotedBlockType::NextVotedBlock);
  load_db_votes(TwoTPlusOneVotedBlockType::NextVotedNullBlock);

  return votes;
}

void DbStorage::removeExtraRewardVotes(const std::vector<vote_hash_t>& votes, Batch& write_batch) {
  for (const auto& v : votes) {
    remove(write_batch, Columns::extra_reward_votes, v.asBytes());
  }
}

void DbStorage::saveExtraRewardVote(const std::shared_ptr<PbftVote>& vote) {
  insert(Columns::extra_reward_votes, vote->getHash().asBytes(), vote->rlp(true, true));
}

std::vector<std::shared_ptr<PbftVote>> DbStorage::getRewardVotes() {
  std::vector<std::shared_ptr<PbftVote>> votes;

  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::extra_reward_votes)));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    votes.emplace_back(std::make_shared<PbftVote>(asBytes(it->value().ToString())));
  }

  return votes;
}

void DbStorage::addPbftBlockPeriodToBatch(PbftPeriod period, taraxa::blk_hash_t const& pbft_block_hash,
                                          Batch& write_batch) {
  insert(write_batch, Columns::pbft_block_period, toSlice(pbft_block_hash.asBytes()), toSlice(period));
}

std::pair<bool, PbftPeriod> DbStorage::getPeriodFromPbftHash(taraxa::blk_hash_t const& pbft_block_hash) {
  auto data = lookup(toSlice(pbft_block_hash.asBytes()), Columns::pbft_block_period);

  if (!data.empty()) {
    PbftPeriod period;
    memcpy(&period, data.data(), sizeof(PbftPeriod));
    return {true, period};
  }

  return {false, 0};
}

std::shared_ptr<std::pair<PbftPeriod, uint32_t>> DbStorage::getDagBlockPeriod(blk_hash_t const& hash) {
  auto data = asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_block_period));
  if (data.size() > 0) {
    dev::RLP rlp(data);
    auto it = rlp.begin();
    auto period = (*it++).toInt<PbftPeriod>();
    auto position = (*it++).toInt<uint32_t>();

    return std::make_shared<std::pair<PbftPeriod, uint32_t>>(period, position);
  }
  return nullptr;
}

void DbStorage::addDagBlockPeriodToBatch(blk_hash_t const& hash, PbftPeriod period, uint32_t position,
                                         Batch& write_batch) {
  dev::RLPStream s;
  s.appendList(2);
  s << period;
  s << position;
  insert(write_batch, Columns::dag_block_period, toSlice(hash.asBytes()), toSlice(s.invalidate()));
}

std::vector<blk_hash_t> DbStorage::getFinalizedDagBlockHashesByPeriod(PbftPeriod period) {
  std::vector<blk_hash_t> ret;
  if (auto period_data = getPeriodDataRaw(period); period_data.size() > 0) {
    auto dag_blocks_data = dev::RLP(period_data)[DAG_BLOCKS_POS_IN_PERIOD_DATA];
    const auto dag_blocks = decodeDAGBlocksBundleRlp(dag_blocks_data);
    ret.reserve(dag_blocks.size());
    std::transform(dag_blocks.begin(), dag_blocks.end(), std::back_inserter(ret),
                   [](const auto& dag_block) { return dag_block->getHash(); });
  }

  return ret;
}

std::vector<std::shared_ptr<DagBlock>> DbStorage::getFinalizedDagBlockByPeriod(PbftPeriod period) {
  auto period_data = getPeriodDataRaw(period);
  if (period_data.empty()) {
    return {};
  }

  auto dag_blocks_data = dev::RLP(period_data)[DAG_BLOCKS_POS_IN_PERIOD_DATA];
  return decodeDAGBlocksBundleRlp(dag_blocks_data);
}

std::pair<blk_hash_t, std::vector<std::shared_ptr<DagBlock>>>
DbStorage::getLastPbftBlockHashAndFinalizedDagBlockByPeriod(PbftPeriod period) {
  auto period_data = getPeriodDataRaw(period);
  if (period_data.empty()) {
    return {};
  }

  const auto period_data_rlp = dev::RLP(period_data);
  auto dag_blocks_data = period_data_rlp[DAG_BLOCKS_POS_IN_PERIOD_DATA];
  auto blocks = decodeDAGBlocksBundleRlp(dag_blocks_data);
  auto last_pbft_block_hash =
      period_data_rlp[PBFT_BLOCK_POS_IN_PERIOD_DATA][PREV_BLOCK_HASH_POS_IN_PBFT_BLOCK].toHash<blk_hash_t>();
  return {last_pbft_block_hash, std::move(blocks)};
}

std::optional<PbftPeriod> DbStorage::getProposalPeriodForDagLevel(uint64_t level) {
  auto it =
      std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::proposal_period_levels_map)));
  it->Seek(toSlice(level));

  // this means that no sortition_params_change in database. It could be met in the tests
  if (!it->Valid()) {
    return {};
  }

  PbftPeriod value;
  memcpy(&value, it->value().data(), sizeof(PbftPeriod));
  return value;
}

void DbStorage::saveProposalPeriodDagLevelsMap(uint64_t level, PbftPeriod period) {
  insert(Columns::proposal_period_levels_map, toSlice(level), toSlice(period));
}

void DbStorage::addProposalPeriodDagLevelsMapToBatch(uint64_t level, PbftPeriod period, Batch& write_batch) {
  insert(write_batch, Columns::proposal_period_levels_map, toSlice(level), toSlice(period));
}

void DbStorage::forEach(Column const& col, OnEntry const& f) {
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(col)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    f(i->key(), i->value());
  }
}

}  // namespace taraxa