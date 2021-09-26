#include "db_storage.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "node/full_node.hpp"
#include "rocksdb/utilities/checkpoint.h"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = std::filesystem;

DbStorage::DbStorage(fs::path const& path, uint32_t db_snapshot_each_n_pbft_block, uint32_t db_max_snapshots,
                     uint32_t db_revert_to_period, addr_t node_addr, bool rebuild)
    : path_(path),
      handles_(Columns::all.size()),
      db_snapshot_each_n_pbft_block_(db_snapshot_each_n_pbft_block),
      db_max_snapshots_(db_max_snapshots),
      node_addr_(node_addr) {
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
  vector<ColumnFamilyDescriptor> descriptors;
  std::transform(Columns::all.begin(), Columns::all.end(), std::back_inserter(descriptors),
                 [](const Column& col) { return ColumnFamilyDescriptor(col.name(), ColumnFamilyOptions()); });
  LOG_OBJECTS_CREATE("DBS");

  // Iterate over the db folders and populate snapshot set
  loadSnapshots();

  // Revert to period if needed
  if (db_revert_to_period) {
    recoverToPeriod(db_revert_to_period);
  }

  checkStatus(rocksdb::DB::Open(options, db_path_.string(), descriptors, &handles_, &db_));
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
  if (db_snapshot_each_n_pbft_block_ > 0 && period % db_snapshot_each_n_pbft_block_ == 0 &&
      snapshots_.find(period) == snapshots_.end()) {
    LOG(log_nf_) << "Creating DB snapshot on period: " << period;

    // Create rocskd checkpoint/snapshot
    rocksdb::Checkpoint* checkpoint;
    auto status = rocksdb::Checkpoint::Create(db_, &checkpoint);
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
  return false;
}

void DbStorage::recoverToPeriod(uint64_t period) {
  LOG(log_nf_) << "Revet to snapshot from period: " << period;

  // Construct the snapshot folder names
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
  period_path += to_string(period);
  period_state_path += to_string(period);

  // Delete both db and state_db folder
  fs::remove_all(period_path);
  LOG(log_dg_) << "Deleted folder: " << period_path;
  fs::remove_all(period_state_path);
  LOG(log_dg_) << "Deleted folder: " << period_path;
}

DbStorage::~DbStorage() {
  for (auto cf : handles_) {
    checkStatus(db_->DestroyColumnFamilyHandle(cf));
  }
  checkStatus(db_->Close());
  delete db_;
}

void DbStorage::checkStatus(rocksdb::Status const& status) {
  if (status.ok()) return;
  throw DbException(string("Db error. Status code: ") + to_string(status.code()) +
                    " SubCode: " + to_string(status.subcode()) + " Message:" + status.ToString());
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
      auto period_data_rlp = RLP(period_data);
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

void DbStorage::updateDagBlockCounters(Batch& write_batch, vector<DagBlock> blks) {
  // Lock is needed since we are editing some fields
  lock_guard<mutex> u_lock(dag_blocks_mutex_);
  for (auto const& blk : blks) {
    auto level = blk.getLevel();
    auto block_hashes = getBlocksByLevel(level);
    block_hashes.emplace(blk.getHash());
    RLPStream blocks_stream(block_hashes.size());
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
}

void DbStorage::saveDagBlock(DagBlock const& blk, Batch* write_batch_p) {
  // Lock is needed since we are editing some fields
  lock_guard<mutex> u_lock(dag_blocks_mutex_);
  auto write_batch_up = write_batch_p ? unique_ptr<Batch>() : make_unique<Batch>();
  auto commit = !write_batch_p;
  auto& write_batch = write_batch_p ? *write_batch_p : *write_batch_up;
  auto block_bytes = blk.rlp(true);
  auto block_hash = blk.getHash();
  insert(write_batch, Columns::dag_blocks, toSlice(block_hash.asBytes()), toSlice(block_bytes));
  auto level = blk.getLevel();
  auto block_hashes = getBlocksByLevel(level);
  block_hashes.emplace(blk.getHash());
  RLPStream blocks_stream(block_hashes.size());
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

void DbStorage::addDagBlockStateToBatch(Batch& write_batch, blk_hash_t const& blk_hash, bool finalized) {
  insert(write_batch, Columns::dag_blocks_state, toSlice(blk_hash.asBytes()), toSlice(finalized));
}

void DbStorage::removeDagBlockStateToBatch(Batch& write_batch, blk_hash_t const& blk_hash) {
  remove(write_batch, Columns::dag_blocks_state, toSlice(blk_hash.asBytes()));
}

std::map<blk_hash_t, bool> DbStorage::getAllDagBlockState() {
  std::map<blk_hash_t, bool> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::dag_blocks_state)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    res[blk_hash_t(asBytes(i->key().ToString()))] = (bool)*(uint8_t*)(i->value().data());
  }
  return res;
}

void DbStorage::savePeriodData(const SyncBlock& sync_block, Batch& write_batch) {
  uint64_t period = sync_block.pbft_blk->getPeriod();
  addPbftBlockPeriodToBatch(period, sync_block.pbft_blk->getBlockHash(), write_batch);

  // Add dag_block_period in DB
  uint64_t block_pos = 0;
  for (auto const& block : sync_block.dag_blocks) {
    addDagBlockPeriodToBatch(block.getHash(), period, block_pos, write_batch);
    block_pos++;
  }

  for (auto const& block : sync_block.dag_blocks) {
    // Remove dag blocks
    remove(write_batch, Columns::dag_blocks, toSlice(block.getHash()));
  }

  insert(write_batch, Columns::period_data, toSlice(period), toSlice(sync_block.rlp()));
}

dev::bytes DbStorage::getPeriodDataRaw(uint64_t period) {
  return asBytes(lookup(toSlice(period), Columns::period_data));
}

void DbStorage::saveTransaction(Transaction const& trx, bool verified) {
  insert(Columns::transactions, toSlice(trx.getHash().asBytes()), toSlice(*trx.rlp(verified)));
}

void DbStorage::saveTransactionStatus(trx_hash_t const& trx_hash, TransactionStatus const& status) {
  insert(Columns::trx_status, toSlice(trx_hash.asBytes()), toSlice(status.rlp()));
}

void DbStorage::addTransactionStatusToBatch(Batch& write_batch, trx_hash_t const& trx,
                                            TransactionStatus const& status) {
  insert(write_batch, Columns::trx_status, toSlice(trx.asBytes()), toSlice(status.rlp()));
}

TransactionStatus DbStorage::getTransactionStatus(trx_hash_t const& hash) {
  auto data = lookup(toSlice(hash.asBytes()), Columns::trx_status);
  if (!data.empty()) {
    dev::RLP const rlp(data);
    return TransactionStatus(rlp);
  }
  return TransactionStatus();
}

std::vector<TransactionStatus> DbStorage::getTransactionStatus(std::vector<trx_hash_t> const& trx_hashes) {
  std::vector<TransactionStatus> result;
  result.reserve(trx_hashes.size());
  DbStorage::MultiGetQuery db_query(shared_from_this(), trx_hashes.size());
  db_query.append(DbStorage::Columns::trx_status, trx_hashes);
  auto db_trxs_statuses = db_query.execute();
  for (size_t idx = 0; idx < db_trxs_statuses.size(); idx++) {
    auto& trx_raw_status = db_trxs_statuses[idx];
    if (!trx_raw_status.empty()) {
      auto data = asBytes(trx_raw_status);
      result.emplace_back(RLP(data));
    }
  }

  return result;
}

std::unordered_map<trx_hash_t, TransactionStatus> DbStorage::getAllTransactionStatus() {
  std::unordered_map<trx_hash_t, TransactionStatus> res;
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::trx_status)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    auto data = asBytes(i->value().ToString());
    dev::RLP const rlp(data);
    res[trx_hash_t(asBytes(i->key().ToString()))] = TransactionStatus(rlp);
  }
  return res;
}

std::optional<PbftBlock> DbStorage::getPbftBlock(uint64_t period) {
  auto period_data = getPeriodDataRaw(period);
  // DB is corrupted if status point to missing or incorrect transaction
  if (period_data.size() > 0) {
    auto period_data_rlp = RLP(period_data);
    return std::optional<PbftBlock>(period_data_rlp[PBFT_BLOCK_POS_IN_PERIOD_DATA]);
  }
  return {};
}

std::shared_ptr<Transaction> DbStorage::getTransaction(trx_hash_t const& hash) {
  auto data = asBytes(lookup(toSlice(hash.asBytes()), Columns::transactions));
  if (data.size() > 0) {
    return std::make_shared<Transaction>(data);
  }
  auto status = getTransactionStatus(hash);
  if (status.state == TransactionStatusEnum::finalized) {
    auto period_data = getPeriodDataRaw(status.period);
    // DB is corrupted if status point to missing or incorrect transaction
    assert(period_data.size() > 0);
    auto period_data_rlp = RLP(period_data);
    auto transaction_data = period_data_rlp[TRANSACTIONS_POS_IN_PERIOD_DATA];
    return std::make_shared<Transaction>(transaction_data[status.position]);
  }
  return nullptr;
}

std::shared_ptr<std::pair<Transaction, taraxa::bytes>> DbStorage::getTransactionExt(trx_hash_t const& hash) {
  auto trx = getTransaction(hash);
  if (trx) {
    return std::make_shared<std::pair<Transaction, taraxa::bytes>>(*trx, *trx->rlp());
  }
  return nullptr;
}

void DbStorage::addTransactionToBatch(Transaction const& trx, Batch& write_batch, bool verified) {
  insert(write_batch, DbStorage::Columns::transactions, toSlice(trx.getHash().asBytes()), toSlice(*trx.rlp(verified)));
}

void DbStorage::removeTransactionToBatch(trx_hash_t const& trx, Batch& write_batch) {
  remove(write_batch, Columns::transactions, toSlice(trx));
}

bool DbStorage::transactionInDb(trx_hash_t const& hash) {
  return exist(toSlice(hash.asBytes()), Columns::transactions);
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

shared_ptr<blk_hash_t> DbStorage::getPbftMgrVotedValue(PbftMgrVotedValue field) {
  auto hash = asBytes(lookup(toSlice(field), Columns::pbft_mgr_voted_value));
  if (hash.size() > 0) {
    return make_shared<blk_hash_t>(hash);
  }
  return nullptr;
}

void DbStorage::savePbftMgrVotedValue(PbftMgrVotedValue field, blk_hash_t const& value) {
  insert(Columns::pbft_mgr_voted_value, toSlice(field), toSlice(value.asBytes()));
}

void DbStorage::addPbftMgrVotedValueToBatch(PbftMgrVotedValue field, blk_hash_t const& value, Batch& write_batch) {
  insert(write_batch, Columns::pbft_mgr_voted_value, toSlice(field), toSlice(value.asBytes()));
}

shared_ptr<PbftBlock> DbStorage::getPbftCertVotedBlock(blk_hash_t const& block_hash) {
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

std::vector<std::shared_ptr<Vote>> DbStorage::getUnverifiedVotes() {
  std::vector<std::shared_ptr<Vote>> votes;

  auto it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(Columns::unverified_votes)));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    votes.emplace_back(std::make_shared<Vote>(asBytes(it->value().ToString())));
  }

  return votes;
}

// Only for test
std::shared_ptr<Vote> DbStorage::getUnverifiedVote(vote_hash_t const& vote_hash) {
  auto vote = asBytes(lookup(toSlice(vote_hash.asBytes()), Columns::unverified_votes));
  if (!vote.empty()) {
    return std::make_shared<Vote>(RLP(vote));
  }
  return nullptr;
}

bool DbStorage::unverifiedVoteExist(vote_hash_t const& vote_hash) {
  auto vote = asBytes(lookup(toSlice(vote_hash.asBytes()), Columns::unverified_votes));
  if (vote.empty()) {
    return false;
  }

  return true;
}

void DbStorage::saveUnverifiedVote(std::shared_ptr<Vote> const& vote) {
  insert(Columns::unverified_votes, toSlice(vote->getHash().asBytes()), toSlice(vote->rlp(true)));
}

void DbStorage::addUnverifiedVoteToBatch(std::shared_ptr<Vote> const& vote, Batch& write_batch) {
  insert(write_batch, Columns::unverified_votes, toSlice(vote->getHash().asBytes()), toSlice(vote->rlp(true)));
}

void DbStorage::removeUnverifiedVoteToBatch(vote_hash_t const& vote_hash, Batch& write_batch) {
  remove(write_batch, Columns::unverified_votes, toSlice(vote_hash.asBytes()));
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
    return std::make_shared<Vote>(RLP(vote));
  }
  return nullptr;
}

void DbStorage::saveVerifiedVote(std::shared_ptr<Vote> const& vote) {
  insert(Columns::verified_votes, toSlice(vote->getHash().asBytes()), toSlice(vote->rlp(true)));
}

void DbStorage::addVerifiedVoteToBatch(std::shared_ptr<Vote> const& vote, Batch& write_batch) {
  insert(write_batch, Columns::verified_votes, toSlice(vote->getHash().asBytes()), toSlice(vote->rlp(true)));
}

void DbStorage::removeVerifiedVoteToBatch(vote_hash_t const& vote_hash, Batch& write_batch) {
  remove(write_batch, Columns::verified_votes, toSlice(vote_hash.asBytes()));
}

std::vector<std::shared_ptr<Vote>> DbStorage::getSoftVotes(uint64_t pbft_round) {
  std::vector<std::shared_ptr<Vote>> soft_votes;
  auto soft_votes_raw = asBytes(lookup(toSlice(pbft_round), Columns::soft_votes));
  auto soft_votes_rlp = RLP(soft_votes_raw);
  soft_votes.reserve(soft_votes_rlp.size());

  for (auto const soft_vote : soft_votes_rlp) {
    soft_votes.emplace_back(std::make_shared<Vote>(soft_vote));
  }

  return soft_votes;
}

// Only for test
void DbStorage::saveSoftVotes(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& soft_votes) {
  RLPStream s(soft_votes.size());
  for (auto const& v : soft_votes) {
    s.appendRaw(v->rlp(true));
  }
  insert(Columns::soft_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::addSoftVotesToBatch(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& soft_votes,
                                    Batch& write_batch) {
  RLPStream s(soft_votes.size());
  for (auto const& v : soft_votes) {
    s.appendRaw(v->rlp(true));
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
    auto period_data_rlp = RLP(period_data);
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
  auto next_votes_rlp = RLP(next_votes_raw);
  next_votes.reserve(next_votes_rlp.size());

  for (auto const next_vote : next_votes_rlp) {
    next_votes.emplace_back(std::make_shared<Vote>(next_vote));
  }

  return next_votes;
}

void DbStorage::saveNextVotes(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& next_votes) {
  RLPStream s(next_votes.size());
  for (auto const& v : next_votes) {
    s.appendRaw(v->rlp(true));
  }
  insert(Columns::next_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::addNextVotesToBatch(uint64_t pbft_round, std::vector<std::shared_ptr<Vote>> const& next_votes,
                                    Batch& write_batch) {
  RLPStream s(next_votes.size());
  for (auto const& v : next_votes) {
    s.appendRaw(v->rlp(true));
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

pair<bool, uint64_t> DbStorage::getPeriodFromPbftHash(taraxa::blk_hash_t const& pbft_block_hash) {
  auto data = lookup(toSlice(pbft_block_hash.asBytes()), Columns::pbft_block_period);

  if (!data.empty()) {
    uint64_t value;
    memcpy(&value, data.data(), sizeof(uint64_t));
    return {true, value};
  }

  return {false, 0};
}

shared_ptr<pair<uint32_t, uint32_t>> DbStorage::getDagBlockPeriod(blk_hash_t const& hash) {
  auto data = asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_block_period));
  if (data.size() > 0) {
    RLP rlp(data);
    auto it = rlp.begin();
    auto period = (*it++).toInt<uint32_t>();
    auto position = (*it++).toInt<uint32_t>();

    return make_shared<pair<uint32_t, uint32_t>>(period, position);
  }
  return nullptr;
}

void DbStorage::addDagBlockPeriodToBatch(blk_hash_t const& hash, uint32_t period, uint32_t position,
                                         Batch& write_batch) {
  RLPStream s;
  s.appendList(2);
  s << period;
  s << position;
  insert(write_batch, Columns::dag_block_period, toSlice(hash.asBytes()), toSlice(s.invalidate()));
}

vector<blk_hash_t> DbStorage::getFinalizedDagBlockHashesByAnchor(blk_hash_t const& anchor) {
  auto raw = lookup(toSlice(anchor), Columns::dag_finalized_blocks);
  if (raw.empty()) {
    return {};
  }
  return RLP(raw).toVector<blk_hash_t>();
}

void DbStorage::putFinalizedDagBlockHashesByAnchor(WriteBatch& b, blk_hash_t const& anchor,
                                                   vector<blk_hash_t> const& hs) {
  insert(b, DbStorage::Columns::dag_finalized_blocks, anchor, RLPStream().appendVector(hs).out());
}

uint64_t DbStorage::getDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus field) {
  auto status = lookup(toSlice(field), Columns::dpos_proposal_period_levels_status);
  if (!status.empty()) {
    uint64_t value;
    memcpy(&value, status.data(), sizeof(uint64_t));
    return value;
  }

  return 0;
}

// Only for test
void DbStorage::saveDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus field, uint64_t value) {
  insert(Columns::dpos_proposal_period_levels_status, toSlice(field), toSlice(value));
}

void DbStorage::addDposProposalPeriodLevelsFieldToBatch(DposProposalPeriodLevelsStatus field, uint64_t value,
                                                        Batch& write_batch) {
  insert(write_batch, Columns::dpos_proposal_period_levels_status, toSlice(field), toSlice(value));
}

bytes DbStorage::getProposalPeriodDagLevelsMap(uint64_t proposal_period) {
  return asBytes(lookup(toSlice(proposal_period), Columns::proposal_period_levels_map));
}

void DbStorage::saveProposalPeriodDagLevelsMap(ProposalPeriodDagLevelsMap const& period_levels_map) {
  insert(Columns::proposal_period_levels_map, toSlice(period_levels_map.proposal_period),
         toSlice(period_levels_map.rlp()));
}

void DbStorage::addProposalPeriodDagLevelsMapToBatch(ProposalPeriodDagLevelsMap const& period_levels_map,
                                                     Batch& write_batch) {
  insert(write_batch, Columns::proposal_period_levels_map, toSlice(period_levels_map.proposal_period),
         toSlice(period_levels_map.rlp()));
}

uint64_t DbStorage::getColumnSize(Column const& col) const {
  rocksdb::ColumnFamilyMetaData data;
  db_->GetColumnFamilyMetaData(handle(col), &data);
  return data.size;
}

void DbStorage::forEach(Column const& col, OnEntry const& f) {
  auto i = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(read_options_, handle(col)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    if (!f(i->key(), i->value())) {
      break;
    }
  }
}

DbStorage::MultiGetQuery::MultiGetQuery(shared_ptr<DbStorage> const& db, uint capacity) : db_(db) {
  if (capacity) {
    cfs_.reserve(capacity);
    keys_.reserve(capacity);
    str_pool_.reserve(capacity);
  }
}

uint DbStorage::MultiGetQuery::size() { return keys_.size(); }

vector<string> DbStorage::MultiGetQuery::execute(bool and_reset) {
  auto _size = size();
  if (_size == 0) {
    return {};
  }
  vector<string> ret(_size);
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
