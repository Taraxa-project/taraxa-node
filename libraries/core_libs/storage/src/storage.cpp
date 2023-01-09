#include "storage/storage.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <memory>

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
                     uint32_t db_max_snapshots, PbftPeriod db_revert_to_period, addr_t node_addr, bool rebuild,
                     bool rebuild_columns)
    : path_(path),
      handles_(Columns::all.size()),
      kDbSnapshotsEachNblock(db_snapshot_each_n_pbft_block),
      kDbSnapshotsMaxCount(db_max_snapshots) {
  db_path_ = (path / kDbDir);
  state_db_path_ = (path / kStateDbDir);

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
  // This options is related to memory consumption
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
  db_->EnableFileDeletions();
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
    if (block.getPivot() != kNullBlockHash) {
      res[block.getLevel()].emplace_back(std::move(block));
    }
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
    if (blk.getPivot() == kNullBlockHash) {
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
  if (blk.getPivot() == kNullBlockHash) {
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

void DbStorage::clearPeriodDataHistory(PbftPeriod end_period) {
  // This is expensive operation but it should not be run more than once a day
  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::period_data)));
  // Find the first non-deleted period
  it->SeekToFirst();
  if (it->Valid()) {
    uint64_t start_period;
    memcpy(&start_period, it->key().data(), sizeof(uint64_t));
    if (start_period < end_period) {
      auto write_batch = createWriteBatch();
      // Delete up to max 10000 period at once
      const uint32_t max_batch_delete = 10000;
      auto start_slice = toSlice(start_period);
      auto end_slice = toSlice(end_period);
      for (auto period = start_period; period < end_period; period++) {
        // Find transactions included in the old blocks and delete data related to these transactions to free disk space
        auto trx_hashes_raw = lookup(period, DB::Columns::final_chain_transaction_hashes_by_blk_number);
        auto hashes_count = trx_hashes_raw.size() / trx_hash_t::size;
        for (uint32_t i = 0; i < hashes_count; i++) {
          auto hash =
              trx_hash_t((uint8_t*)(trx_hashes_raw.data() + i * trx_hash_t::size), trx_hash_t::ConstructFromPointer);
          remove(write_batch, Columns::final_chain_receipt_by_trx_hash, hash);
          remove(write_batch, Columns::final_chain_transaction_location_by_hash, hash);
        }
        remove(write_batch, Columns::final_chain_transaction_hashes_by_blk_number, EthBlockNumber(period));
        if ((period - start_period + 1) % max_batch_delete == 0) {
          commitWriteBatch(write_batch);
          write_batch = createWriteBatch();
        }
      }
      commitWriteBatch(write_batch);

      db_->DeleteRange(write_options_, handle(Columns::period_data), start_slice, end_slice);
      // Deletion alone does not guarantee that the disk space is freed, these CompactRange methods actually compact the
      // data in the database and free disk space
      db_->CompactRange({}, handle(Columns::period_data), &start_slice, &end_slice);
      db_->CompactRange({}, handle(Columns::final_chain_receipt_by_trx_hash), nullptr, nullptr);
      db_->CompactRange({}, handle(Columns::final_chain_transaction_location_by_hash), nullptr, nullptr);
      db_->CompactRange({}, handle(Columns::final_chain_transaction_hashes_by_blk_number), nullptr, nullptr);
    }
  }
}

void DbStorage::savePeriodData(const PeriodData& period_data, Batch& write_batch) {
  const auto period = period_data.pbft_blk->getPeriod();
  addPbftBlockPeriodToBatch(period, period_data.pbft_blk->getBlockHash(), write_batch);

  // Remove dag blocks from non finalized column in db and add dag_block_period in DB
  uint32_t block_pos = 0;
  for (auto const& block : period_data.dag_blocks) {
    removeDagBlockBatch(write_batch, block.getHash());
    addDagBlockPeriodToBatch(block.getHash(), period, block_pos, write_batch);
    block_pos++;
  }

  // Remove transactions from non finalized column in db and add dag_block_period in DB
  uint32_t trx_pos = 0;
  for (auto const& trx : period_data.transactions) {
    removeTransactionToBatch(trx->getHash(), write_batch);
    addTransactionPeriodToBatch(write_batch, trx->getHash(), period_data.pbft_blk->getPeriod(), trx_pos);
    trx_pos++;
  }

  insert(write_batch, Columns::period_data, toSlice(period), toSlice(period_data.rlp()));
}

dev::bytes DbStorage::getPeriodDataRaw(PbftPeriod period) const {
  return asBytes(lookup(toSlice(period), Columns::period_data));
}

void DbStorage::saveTransaction(Transaction const& trx) {
  insert(Columns::transactions, toSlice(trx.getHash().asBytes()), toSlice(trx.rlp()));
}

void DbStorage::saveTransactionPeriod(trx_hash_t const& trx_hash, PbftPeriod period, uint32_t position) {
  dev::RLPStream s;
  s.appendList(2);
  s << period;
  s << position;
  insert(Columns::trx_period, toSlice(trx_hash.asBytes()), toSlice(s.out()));
}

void DbStorage::addTransactionPeriodToBatch(Batch& write_batch, trx_hash_t const& trx, PbftPeriod period,
                                            uint32_t position) {
  dev::RLPStream s;
  s.appendList(2);
  s << period;
  s << position;
  insert(write_batch, Columns::trx_period, toSlice(trx.asBytes()), toSlice(s.out()));
}

std::optional<std::pair<PbftPeriod, uint32_t>> DbStorage::getTransactionPeriod(trx_hash_t const& hash) const {
  auto data = lookup(toSlice(hash.asBytes()), Columns::trx_period);
  if (!data.empty()) {
    std::pair<PbftPeriod, uint32_t> res;
    dev::RLP const rlp(data);
    auto it = rlp.begin();
    res.first = (*it++).toInt<PbftPeriod>();
    res.second = (*it++).toInt<uint32_t>();
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
  dev::RLPStream s;
  s.appendList(2);
  s.appendRaw(block->rlp(true));
  s << 0;
  insert(Columns::proposed_pbft_blocks, block->getBlockHash().asBytes(), s.out());
}

void DbStorage::removeProposedPbftBlock(const blk_hash_t& block_hash, Batch& write_batch) {
  remove(write_batch, Columns::proposed_pbft_blocks, toSlice(block_hash.asBytes()));
}

std::vector<std::shared_ptr<PbftBlock>> DbStorage::getProposedPbftBlocks() {
  std::vector<std::shared_ptr<PbftBlock>> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::proposed_pbft_blocks)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    auto data = asBytes(i->value().ToString());
    const dev::RLP rlp(data);
    res.push_back(std::make_shared<PbftBlock>(rlp[0]));
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

std::pair<std::optional<SharedTransactions>, trx_hash_t> DbStorage::getFinalizedTransactions(
    std::vector<trx_hash_t> const& trx_hashes) const {
  // Map of period to position of transactions within a period
  std::map<PbftPeriod, std::set<uint32_t>> period_map;
  SharedTransactions transactions;
  transactions.reserve(trx_hashes.size());
  for (auto const& tx_hash : trx_hashes) {
    auto trx_period = getTransactionPeriod(tx_hash);
    if (trx_period.has_value()) {
      period_map[trx_period->first].insert(trx_period->second);
    } else {
      return {std::nullopt, tx_hash};
    }
  }
  for (auto it : period_map) {
    const auto period_data = getPeriodDataRaw(it.first);
    if (!period_data.size()) {
      assert(false);
    }

    auto const transactions_rlp = dev::RLP(period_data)[TRANSACTIONS_POS_IN_PERIOD_DATA];
    for (auto pos : it.second) {
      transactions.emplace_back(std::make_shared<Transaction>(transactions_rlp[pos]));
    }
  }

  return {transactions, {}};
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

void DbStorage::saveSoftVotedBlockDataInRound(const TwoTPlusOneSoftVotedBlockData& soft_voted_block_data) {
  insert(Columns::soft_voted_block_in_round, 0, toSlice(soft_voted_block_data.rlp()));
}

std::optional<TwoTPlusOneSoftVotedBlockData> DbStorage::getSoftVotedBlockDataInRound() const {
  auto value = asBytes(lookup(0, Columns::soft_voted_block_in_round));
  if (value.empty()) {
    return {};
  }

  return TwoTPlusOneSoftVotedBlockData(dev::RLP(value));
}

void DbStorage::removeSoftVotedBlockDataInRound(Batch& write_batch) {
  remove(write_batch, Columns::soft_voted_block_in_round, 0);
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

std::vector<std::shared_ptr<Vote>> DbStorage::getCertVotes(PbftPeriod period) {
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

std::vector<std::shared_ptr<Vote>> DbStorage::getPreviousRoundNextVotes() {
  std::vector<std::shared_ptr<Vote>> next_votes;
  auto next_votes_raw = asBytes(lookup(0, Columns::next_votes));
  auto next_votes_rlp = dev::RLP(next_votes_raw);
  next_votes.reserve(next_votes_rlp.size());

  for (auto const next_vote : next_votes_rlp) {
    next_votes.emplace_back(std::make_shared<Vote>(next_vote));
  }

  return next_votes;
}

void DbStorage::savePreviousRoundNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes) {
  dev::RLPStream s(next_votes.size());
  for (auto const& v : next_votes) {
    s.appendRaw(v->rlp(true, true));
  }
  insert(Columns::next_votes, 0, toSlice(s.out()));
}

void DbStorage::removePreviousRoundNextVotes() { remove(Columns::next_votes, 0); }

void DbStorage::saveLastBlockCertVote(const std::shared_ptr<Vote>& cert_vote) {
  insert(Columns::last_block_cert_votes, toSlice(cert_vote->getHash()), toSlice(cert_vote->rlp(true, true)));
}

void DbStorage::addLastBlockCertVotesToBatch(
    std::vector<std::shared_ptr<Vote>> const& cert_votes,
    std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> const& old_cert_votes, Batch& write_batch) {
  for (auto const& v : old_cert_votes) {
    remove(write_batch, Columns::last_block_cert_votes, toSlice(v.second->getHash()));
  }

  dev::RLPStream s(cert_votes.size());
  for (auto const& v : cert_votes) {
    insert(write_batch, Columns::last_block_cert_votes, toSlice(v->getHash()), toSlice(v->rlp(true, true)));
  }
}

std::vector<std::shared_ptr<Vote>> DbStorage::getLastBlockCertVotes() {
  std::vector<std::shared_ptr<Vote>> votes;
  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::last_block_cert_votes)));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    votes.emplace_back(std::make_shared<Vote>(asBytes(it->value().ToString())));
  }
  return votes;
}

void DbStorage::removeLastBlockCertVotes(const vote_hash_t& hash) {
  remove(Columns::last_block_cert_votes, toSlice(hash));
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
    ret.reserve(dag_blocks_data.size());
    std::transform(dag_blocks_data.begin(), dag_blocks_data.end(), std::back_inserter(ret),
                   [](const auto& dag_block) { return DagBlock(dag_block).getHash(); });
  }

  return ret;
}

std::vector<std::shared_ptr<DagBlock>> DbStorage::getFinalizedDagBlockByPeriod(PbftPeriod period) {
  std::vector<std::shared_ptr<DagBlock>> ret;
  if (auto period_data = getPeriodDataRaw(period); period_data.size() > 0) {
    auto dag_blocks_data = dev::RLP(period_data)[DAG_BLOCKS_POS_IN_PERIOD_DATA];
    ret.reserve(dag_blocks_data.size());
    for (auto const block : dag_blocks_data) {
      ret.emplace_back(std::make_shared<DagBlock>(block));
    }
  }
  return ret;
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
