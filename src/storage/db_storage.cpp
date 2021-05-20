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
      // First - lazy init default column for rocksdb - must be called before accessing rocksdb because of static init
      // order fail !!! For handles_ initialization is used comma-operator that evaluates first expression, but uses
      // second expression(Columns::all.size()) as return value See:
      // https://en.cppreference.com/w/cpp/language/operator_other#Built-in_comma_operator
      handles_((Columns::Default_column(), Columns::all.size())),
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
  vector<ColumnFamilyDescriptor> descriptors;
  std::transform(Columns::all.begin(), Columns::all.end(), std::back_inserter(descriptors),
                 [](const Column& col) { return ColumnFamilyDescriptor(col.name, ColumnFamilyOptions()); });
  LOG_OBJECTS_CREATE("DBS");

  // Iterate over the db folders and populate snapshot set
  loadSnapshots();

  // Revert to period if needed
  if (db_revert_to_period) {
    recoverToPeriod(db_revert_to_period);
  }

  checkStatus(DB::Open(options, db_path_.string(), descriptors, &handles_, &db_));
  dag_blocks_count_.store(getStatusField(StatusDbField::DagBlkCount));
  dag_edge_count_.store(getStatusField(StatusDbField::DagEdgeCount));

  auto major_version = getStatusField(StatusDbField::DbMajorVersion);
  auto minor_version = getStatusField(StatusDbField::DbMinorVersion);
  if (major_version == 0 && minor_version == 0) {
    saveStatusField(StatusDbField::DbMajorVersion, FullNode::c_database_major_version);
    saveStatusField(StatusDbField::DbMinorVersion, FullNode::c_database_minor_version);
  } else {
    if (major_version != FullNode::c_database_major_version) {
      throw DbException(string("Database version mismatch. Version on disk ") +
                        getFormattedVersion(major_version, minor_version) + " Node version:" +
                        getFormattedVersion(FullNode::c_database_major_version, FullNode::c_database_minor_version));
    } else if (minor_version != FullNode::c_database_minor_version) {
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

bool DbStorage::createSnapshot(uint64_t const& period) {
  // Only creates snapshot each db_snapshot_each_n_pbft_block_ periods
  if (db_snapshot_each_n_pbft_block_ > 0 && period % db_snapshot_each_n_pbft_block_ == 0 &&
      snapshots_.find(period) == snapshots_.end()) {
    LOG(log_nf_) << "Creating DB snapshot on period: " << period;

    // Create rocskd checkpoint/snapshot
    rocksdb::Checkpoint* checkpoint;
    auto status = rocksdb::Checkpoint::Create(db_, &checkpoint);
    // Scope is to delete checkpoint object as soon as we don't need it anymore
    {
      unique_ptr<rocksdb::Checkpoint> realPtr = unique_ptr<rocksdb::Checkpoint>(checkpoint);
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

void DbStorage::recoverToPeriod(uint64_t const& period) {
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

void DbStorage::deleteSnapshot(uint64_t const& period) {
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

DbStorage::BatchPtr DbStorage::createWriteBatch() { return s_ptr(new WriteBatch()); }

void DbStorage::remove(Slice key, Column const& column) {
  auto status = db_->Delete(write_options_, handle(column), key);
  checkStatus(status);
}

void DbStorage::commitWriteBatch(BatchPtr const& write_batch, rocksdb::WriteOptions const& opts) {
  auto status = db_->Write(opts, write_batch->GetWriteBatch());
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

std::string DbStorage::getBlocksByLevel(level_t level) { return lookup(toSlice(level), Columns::dag_blocks_index); }

std::vector<std::shared_ptr<DagBlock>> DbStorage::getDagBlocksAtLevel(level_t level, int number_of_levels) {
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
  batch_put(write_batch, Columns::dag_blocks, toSlice(block_hash.asBytes()), toSlice(block_bytes));
  auto level = blk.getLevel();
  std::string blocks = getBlocksByLevel(level);
  if (blocks == "") {
    blocks = blk.getHash().toString();
  } else {
    blocks = blocks + "," + blk.getHash().toString();
  }
  batch_put(write_batch, Columns::dag_blocks_index, toSlice(level), toSlice(blocks));
  dag_blocks_count_.fetch_add(1);
  batch_put(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagBlkCount),
            toSlice(dag_blocks_count_.load()));
  // Do not count genesis pivot field
  if (blk.getPivot() == blk_hash_t(0)) {
    dag_edge_count_.fetch_add(blk.getTips().size());
  } else {
    dag_edge_count_.fetch_add(blk.getTips().size() + 1);
  }
  batch_put(write_batch, Columns::status, toSlice((uint8_t)StatusDbField::DagEdgeCount),
            toSlice(dag_edge_count_.load()));
  if (commit) {
    commitWriteBatch(write_batch);
  }
}

void DbStorage::addDagBlockStateToBatch(BatchPtr const& write_batch, blk_hash_t const& blk_hash, bool finalized) {
  batch_put(write_batch, Columns::dag_blocks_state, toSlice(blk_hash.asBytes()), toSlice(finalized));
}

void DbStorage::removeDagBlockStateToBatch(BatchPtr const& write_batch, blk_hash_t const& blk_hash) {
  batch_delete(write_batch, Columns::dag_blocks_state, toSlice(blk_hash.asBytes()));
}

std::map<blk_hash_t, bool> DbStorage::getAllDagBlockState() {
  std::map<blk_hash_t, bool> res;
  auto i = u_ptr(db_->NewIterator(read_options_, handle(Columns::dag_blocks_state)));
  for (i->SeekToFirst(); i->Valid(); i->Next()) {
    res[blk_hash_t(asBytes(i->key().ToString()))] = (bool)*(uint8_t*)(i->value().data());
  }
  return res;
}

void DbStorage::saveTransaction(Transaction const& trx) {
  insert(Columns::transactions, toSlice(trx.getHash().asBytes()), toSlice(*trx.rlp()));
}

void DbStorage::saveTransactionStatus(trx_hash_t const& trx_hash, TransactionStatus const& status) {
  insert(Columns::trx_status, toSlice(trx_hash.asBytes()), toSlice((uint16_t)status));
}

void DbStorage::addTransactionStatusToBatch(BatchPtr const& write_batch, trx_hash_t const& trx,
                                            TransactionStatus const& status) {
  batch_put(write_batch, Columns::trx_status, toSlice(trx.asBytes()), toSlice((uint16_t)status));
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
    res[trx_hash_t(asBytes(i->key().ToString()))] = (TransactionStatus) * (uint16_t*)(i->value().data());
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

std::shared_ptr<std::pair<Transaction, taraxa::bytes>> DbStorage::getTransactionExt(trx_hash_t const& hash) {
  auto trx_bytes = asBytes(lookup(toSlice(hash.asBytes()), Columns::transactions));
  if (trx_bytes.size() > 0) {
    return std::make_shared<std::pair<Transaction, taraxa::bytes>>(trx_bytes, trx_bytes);
  }
  return nullptr;
}

void DbStorage::addTransactionToBatch(Transaction const& trx, BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::transactions, toSlice(trx.getHash().asBytes()), toSlice(*trx.rlp()));
}

bool DbStorage::transactionInDb(trx_hash_t const& hash) {
  return !lookup(toSlice(hash.asBytes()), Columns::transactions).empty();
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

void DbStorage::saveStatusField(StatusDbField const& field, uint64_t const& value) {
  insert(Columns::status, toSlice((uint8_t)field), toSlice(value));
}

void DbStorage::addStatusFieldToBatch(StatusDbField const& field, uint64_t const& value, BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::status, toSlice((uint8_t)field), toSlice(value));
}

// PBFT
uint64_t DbStorage::getPbftMgrField(PbftMgrRoundStep const& field) {
  auto status = lookup(toSlice((uint8_t)field), Columns::pbft_mgr_round_step);
  if (!status.empty()) {
    uint64_t value;
    memcpy(&value, status.data(), sizeof(uint64_t));
    return value;
  }

  return 1;
}

void DbStorage::savePbftMgrField(PbftMgrRoundStep const& field, uint64_t const& value) {
  insert(Columns::pbft_mgr_round_step, toSlice((uint8_t)field), toSlice(value));
}

void DbStorage::addPbftMgrFieldToBatch(PbftMgrRoundStep const& field, uint64_t const& value,
                                       BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::pbft_mgr_round_step, toSlice((uint8_t)field), toSlice(value));
}

size_t DbStorage::getPbft2TPlus1(uint64_t const& round) {
  auto status = lookup(toSlice(round), Columns::pbft_round_2t_plus_1);

  if (!status.empty()) {
    size_t value;
    memcpy(&value, status.data(), sizeof(size_t));
    return value;
  }

  return 0;
}

void DbStorage::savePbft2TPlus1(uint64_t const& pbft_round, size_t const& pbft_2t_plus_1) {
  insert(Columns::pbft_round_2t_plus_1, toSlice(pbft_round), toSlice(pbft_2t_plus_1));
}

void DbStorage::addPbft2TPlus1ToBatch(uint64_t const& pbft_round, size_t const& pbft_2t_plus_1,
                                      BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::pbft_round_2t_plus_1, toSlice(pbft_round), toSlice(pbft_2t_plus_1));
}

bool DbStorage::getPbftMgrStatus(PbftMgrStatus const& field) {
  auto status = lookup(toSlice((uint8_t)field), Columns::pbft_mgr_status);
  if (!status.empty()) {
    return *(bool*)&status[0];
  }
  return false;
}

void DbStorage::savePbftMgrStatus(PbftMgrStatus const& field, bool const& value) {
  insert(Columns::pbft_mgr_status, toSlice((uint8_t)field), toSlice(value));
}

void DbStorage::addPbftMgrStatusToBatch(PbftMgrStatus const& field, bool const& value, BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::pbft_mgr_status, toSlice((uint8_t)field), toSlice(value));
}

shared_ptr<blk_hash_t> DbStorage::getPbftMgrVotedValue(PbftMgrVotedValue const& field) {
  auto hash = asBytes(lookup(toSlice((uint8_t)field), Columns::pbft_mgr_voted_value));
  if (hash.size() > 0) {
    return make_shared<blk_hash_t>(hash);
  }
  return nullptr;
}

void DbStorage::savePbftMgrVotedValue(PbftMgrVotedValue const& field, blk_hash_t const& value) {
  insert(Columns::pbft_mgr_voted_value, toSlice((uint8_t)field), toSlice(value.asBytes()));
}

void DbStorage::addPbftMgrVotedValueToBatch(PbftMgrVotedValue const& field, blk_hash_t const& value,
                                            BatchPtr const& write_batch) {
  batch_put(write_batch, Columns::pbft_mgr_voted_value, toSlice((uint8_t)field), toSlice(value.asBytes()));
}

shared_ptr<blk_hash_t> DbStorage::getPbftCertVotedBlockHash(uint64_t const& pbft_round) {
  auto hash = asBytes(lookup(toSlice(pbft_round), Columns::pbft_cert_voted_block_hash));
  if (hash.size() > 0) {
    return make_shared<blk_hash_t>(hash);
  }
  return nullptr;
}

void DbStorage::savePbftCertVotedBlockHash(uint64_t const& pbft_round, blk_hash_t const& cert_voted_block_hash) {
  insert(Columns::pbft_cert_voted_block_hash, toSlice(pbft_round), toSlice(cert_voted_block_hash.asBytes()));
}

void DbStorage::addPbftCertVotedBlockHashToBatch(uint64_t const& pbft_round, blk_hash_t const& cert_voted_block_hash,
                                                 BatchPtr const& write_batch) {
  batch_put(*write_batch, Columns::pbft_cert_voted_block_hash, toSlice(pbft_round),
            toSlice(cert_voted_block_hash.asBytes()));
}

shared_ptr<PbftBlock> DbStorage::getPbftCertVotedBlock(blk_hash_t const& block_hash) {
  auto block = lookup(toSlice(block_hash.asBytes()), Columns::pbft_cert_voted_block);
  if (!block.empty()) {
    return s_ptr(new PbftBlock(dev::RLP(block)));
  }
  return nullptr;
}

void DbStorage::savePbftCertVotedBlock(PbftBlock const& pbft_block) {
  insert(Columns::pbft_cert_voted_block, toSlice(pbft_block.getBlockHash().asBytes()), toSlice(pbft_block.rlp(true)));
}

void DbStorage::addPbftCertVotedBlockToBatch(PbftBlock const& pbft_block, BatchPtr const& write_batch) {
  batch_put(*write_batch, Columns::pbft_cert_voted_block, toSlice(pbft_block.getBlockHash().asBytes()),
            toSlice(pbft_block.rlp(true)));
}

std::shared_ptr<PbftBlock> DbStorage::getPbftBlock(blk_hash_t const& hash) {
  auto block = lookup(hash, Columns::pbft_blocks);
  if (!block.empty()) {
    return s_ptr(new PbftBlock(dev::RLP(block)));
  }
  return nullptr;
}

bool DbStorage::pbftBlockInDb(blk_hash_t const& hash) { return !lookup(hash, Columns::pbft_blocks).empty(); }

void DbStorage::addPbftBlockToBatch(const taraxa::PbftBlock& pbft_block,
                                    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(*write_batch, Columns::pbft_blocks, pbft_block.getBlockHash(), pbft_block.rlp(true));
}

string DbStorage::getPbftHead(blk_hash_t const& hash) { return lookup(toSlice(hash.asBytes()), Columns::pbft_head); }

void DbStorage::savePbftHead(blk_hash_t const& hash, string const& pbft_chain_head_str) {
  insert(Columns::pbft_head, toSlice(hash.asBytes()), pbft_chain_head_str);
}

void DbStorage::addPbftHeadToBatch(taraxa::blk_hash_t const& head_hash, std::string const& head_str,
                                   const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::pbft_head, toSlice(head_hash.asBytes()), head_str);
}

std::vector<Vote> DbStorage::getUnverifiedVotes() {
  vector<Vote> votes;

  auto it = u_ptr(db_->NewIterator(read_options_, handle(Columns::unverified_votes)));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    votes.emplace_back(asBytes(it->value().ToString()));
  }

  return votes;
}

shared_ptr<Vote> DbStorage::getUnverifiedVote(vote_hash_t const& vote_hash) {
  auto vote = asBytes(lookup(toSlice(vote_hash.asBytes()), Columns::unverified_votes));
  if (!vote.empty()) {
    return s_ptr(new Vote(RLP(vote)));
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

void DbStorage::saveUnverifiedVote(Vote const& vote) {
  insert(Columns::unverified_votes, toSlice(vote.getHash().asBytes()), toSlice(vote.rlp(true)));
}

void DbStorage::addUnverifiedVoteToBatch(Vote const& vote, BatchPtr const& write_batch) {
  batch_put(write_batch, Columns::unverified_votes, toSlice(vote.getHash().asBytes()), toSlice(vote.rlp(true)));
}

void DbStorage::removeUnverifiedVoteToBatch(vote_hash_t const& vote_hash, BatchPtr const& write_batch) {
  batch_delete(write_batch, Columns::unverified_votes, toSlice(vote_hash.asBytes()));
}

std::vector<Vote> DbStorage::getVerifiedVotes() {
  vector<Vote> votes;

  auto it = u_ptr(db_->NewIterator(read_options_, handle(Columns::verified_votes)));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    votes.emplace_back(asBytes(it->value().ToString()));
  }

  return votes;
}

shared_ptr<Vote> DbStorage::getVerifiedVote(vote_hash_t const& vote_hash) {
  auto vote = asBytes(lookup(toSlice(vote_hash.asBytes()), Columns::verified_votes));
  if (!vote.empty()) {
    return s_ptr(new Vote(RLP(vote)));
  }
  return nullptr;
}

void DbStorage::saveVerifiedVote(Vote const& vote) {
  insert(Columns::verified_votes, toSlice(vote.getHash().asBytes()), toSlice(vote.rlp(true)));
}

void DbStorage::addVerifiedVoteToBatch(Vote const& vote, BatchPtr const& write_batch) {
  batch_put(write_batch, Columns::verified_votes, toSlice(vote.getHash().asBytes()), toSlice(vote.rlp(true)));
}

void DbStorage::removeVerifiedVoteToBatch(vote_hash_t const& vote_hash, BatchPtr const& write_batch) {
  batch_delete(write_batch, Columns::verified_votes, toSlice(vote_hash.asBytes()));
}

std::vector<Vote> DbStorage::getSoftVotes(uint64_t const& pbft_round) {
  std::vector<Vote> soft_votes;
  auto soft_votes_raw = asBytes(lookup(toSlice(pbft_round), Columns::soft_votes));

  for (auto const& soft_vote : RLP(soft_votes_raw)) {
    soft_votes.emplace_back(soft_vote);
  }

  return soft_votes;
}

void DbStorage::saveSoftVotes(uint64_t const& pbft_round, std::vector<Vote> const& soft_votes) {
  RLPStream s(soft_votes.size());
  for (auto const& v : soft_votes) {
    s.appendRaw(v.rlp(true));
  }
  insert(Columns::soft_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::addSoftVotesToBatch(uint64_t const& pbft_round, std::vector<Vote> const& soft_votes,
                                    BatchPtr const& write_batch) {
  RLPStream s(soft_votes.size());
  for (auto const& v : soft_votes) {
    s.appendRaw(v.rlp(true));
  }
  batch_put(write_batch, Columns::soft_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::removeSoftVotesToBatch(uint64_t const& pbft_round, BatchPtr const& write_batch) {
  batch_delete(write_batch, Columns::soft_votes, toSlice(pbft_round));
}

std::vector<Vote> DbStorage::getCertVotes(blk_hash_t const& hash) {
  std::vector<Vote> cert_votes;
  auto cert_votes_raw = asBytes(lookup(toSlice(hash.asBytes()), Columns::cert_votes));

  for (auto const& cert_vote : RLP(cert_votes_raw)) {
    cert_votes.emplace_back(cert_vote);
  }

  return cert_votes;
}

void DbStorage::addCertVotesToBatch(const taraxa::blk_hash_t& pbft_block_hash, const std::vector<Vote>& cert_votes,
                                    const taraxa::DbStorage::BatchPtr& write_batch) {
  RLPStream s(cert_votes.size());
  for (auto const& v : cert_votes) {
    s.appendRaw(v.rlp(true));
  }
  batch_put(*write_batch, Columns::cert_votes, toSlice(pbft_block_hash.asBytes()), toSlice(s.out()));
}

std::vector<Vote> DbStorage::getNextVotes(uint64_t const& pbft_round) {
  std::vector<Vote> next_votes;
  auto next_votes_raw = asBytes(lookup(toSlice(pbft_round), Columns::next_votes));

  for (auto const& next_vote : RLP(next_votes_raw)) {
    next_votes.emplace_back(next_vote);
  }

  return next_votes;
}

void DbStorage::saveNextVotes(uint64_t const& pbft_round, std::vector<Vote> const& next_votes) {
  RLPStream s(next_votes.size());
  for (auto const& v : next_votes) {
    s.appendRaw(v.rlp(true));
  }
  insert(Columns::next_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::addNextVotesToBatch(uint64_t const& pbft_round, std::vector<Vote> const& next_votes,
                                    BatchPtr const& write_batch) {
  RLPStream s(next_votes.size());
  for (auto const& v : next_votes) {
    s.appendRaw(v.rlp(true));
  }
  batch_put(*write_batch, Columns::next_votes, toSlice(pbft_round), toSlice(s.out()));
}

void DbStorage::removeNextVotesToBatch(uint64_t const& pbft_round, BatchPtr const& write_batch) {
  batch_delete(write_batch, Columns::next_votes, toSlice(pbft_round));
}

shared_ptr<blk_hash_t> DbStorage::getPeriodPbftBlock(uint64_t const& period) {
  auto hash = asBytes(lookup(toSlice(period), Columns::period_pbft_block));
  if (hash.size() > 0) {
    return make_shared<blk_hash_t>(hash);
  }
  return nullptr;
}

void DbStorage::addPbftBlockPeriodToBatch(uint64_t const& period, taraxa::blk_hash_t const& pbft_block_hash,
                                          const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::period_pbft_block, toSlice(period), toSlice(pbft_block_hash.asBytes()));
}

std::shared_ptr<uint64_t> DbStorage::getDagBlockPeriod(blk_hash_t const& hash) {
  auto period = asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_block_period));
  if (period.size() > 0) {
    return make_shared<uint64_t>(*(uint64_t*)&period[0]);
  }
  return nullptr;
}

void DbStorage::addDagBlockPeriodToBatch(blk_hash_t const& hash, uint64_t const& period, BatchPtr const& write_batch) {
  batch_put(write_batch, Columns::dag_block_period, toSlice(hash.asBytes()), toSlice(period));
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
  batch_put(b, DbStorage::Columns::dag_finalized_blocks, anchor, RLPStream().appendVector(hs).out());
}

uint64_t DbStorage::getDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus const& field) {
  auto status = lookup(toSlice((uint8_t)field), Columns::dpos_proposal_period_levels_status);
  if (!status.empty()) {
    uint64_t value;
    memcpy(&value, status.data(), sizeof(uint64_t));
    return value;
  }

  return 0;
}

void DbStorage::saveDposProposalPeriodLevelsField(DposProposalPeriodLevelsStatus const& field, uint64_t const& value) {
  insert(Columns::dpos_proposal_period_levels_status, toSlice((uint8_t)field), toSlice(value));
}

void DbStorage::addDposProposalPeriodLevelsFieldToBatch(DposProposalPeriodLevelsStatus const& field,
                                                        uint64_t const& value, BatchPtr const& write_batch) {
  batch_put(write_batch, Columns::dpos_proposal_period_levels_status, toSlice((uint8_t)field), toSlice(value));
}

bytes DbStorage::getProposalPeriodDagLevelsMap(uint64_t proposal_period) {
  return asBytes(lookup(toSlice(proposal_period), Columns::proposal_period_levels_map));
}

void DbStorage::saveProposalPeriodDagLevelsMap(ProposalPeriodDagLevelsMap const& period_levels_map) {
  insert(Columns::proposal_period_levels_map, toSlice(period_levels_map.proposal_period),
         toSlice(period_levels_map.rlp()));
}

void DbStorage::addProposalPeriodDagLevelsMapToBatch(ProposalPeriodDagLevelsMap const& period_levels_map,
                                                     BatchPtr const& write_batch) {
  batch_put(write_batch, Columns::proposal_period_levels_map, toSlice(period_levels_map.proposal_period),
            toSlice(period_levels_map.rlp()));
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

DbStorage::MultiGetQuery::MultiGetQuery(shared_ptr<DbStorage> const& db, uint capacity) : db_(db) {
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
