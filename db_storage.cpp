#include "db_storage.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "transaction.hpp"
#include "vote.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = boost::filesystem;

DbStorage::DbStorage(fs::path const& path)
    : path_(path), handles_(Columns::all.size()) {
  fs::create_directories(path);
  rocksdb::Options options;
  options.create_missing_column_families = true;
  options.create_if_missing = true;
  options.max_open_files = 256;
  vector<ColumnFamilyDescriptor> descriptors;
  std::transform(Columns::all.begin(), Columns::all.end(),
                 std::back_inserter(descriptors), [](const Column& col) {
                   return ColumnFamilyDescriptor(col.name,
                                                 ColumnFamilyOptions());
                 });
  checkStatus(DB::Open(options, path.string(), descriptors, &handles_, &db_));
  dag_blocks_count_.store(getStatusField(StatusDbField::DagBlkCount));
  dag_edge_count_.store(getStatusField(StatusDbField::DagEdgeCount));
}

DbStorage::~DbStorage() {
  db_->Close();
  delete db_;
}

void DbStorage::checkStatus(rocksdb::Status const& status) {
  if (status.ok()) return;
  throw DbException((string("Db error. Status code: ") +
                     to_string(status.code()) +
                     " SubCode: " + to_string(status.subcode()) +
                     " Message:" + status.ToString())
                        .c_str());
}

DbStorage::BatchPtr DbStorage::createWriteBatch() {
  return DbStorage::BatchPtr(new WriteBatch());
}

std::string DbStorage::lookup(Slice key, Column const& column) {
  std::string value;
  Status status = db_->Get(read_options_, handle(column), key, &value);
  if (status.IsNotFound()) return std::string();
  checkStatus(status);
  return value;
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

void DbStorage::saveDagBlock(DagBlock const& blk) {
  // Lock is needed since we are editing some fields
  lock_guard<mutex> u_lock(dag_blocks_mutex_);
  auto write_batch = createWriteBatch();
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

  commitWriteBatch(write_batch);
}

void DbStorage::saveDagBlockState(blk_hash_t const& blk_hash, bool finalized) {
  insert(Columns::dag_blocks_state, toSlice(blk_hash.asBytes()),
         toSlice(finalized));
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

void DbStorage::saveTransactionToBlock(trx_hash_t const& trx_hash,
                                       blk_hash_t const& blk_hash) {
  insert(Columns::trx_to_blk, trx_hash.toString(), blk_hash.toString());
}

std::shared_ptr<blk_hash_t> DbStorage::getTransactionToBlock(
    trx_hash_t const& hash) {
  auto blk_hash = lookup(hash.toString(), Columns::trx_to_blk);
  if (!blk_hash.empty()) {
    return make_shared<blk_hash_t>(blk_hash);
  }
  return nullptr;
}

bool DbStorage::transactionToBlockInDb(trx_hash_t const& hash) {
  return !lookup(hash.toString(), Columns::trx_to_blk).empty();
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
  auto block = lookup(toSlice(hash.asBytes()), Columns::pbft_blocks);
  if (!block.empty()) {
    return std::make_shared<PbftBlock>(block);
  }
  return nullptr;
}

void DbStorage::savePbftBlock(PbftBlock const& block) {
  insert(Columns::pbft_blocks, toSlice(block.getBlockHash().asBytes()),
         block.getJsonStr());
}

bool DbStorage::pbftBlockInDb(blk_hash_t const& hash) {
  return !lookup(toSlice(hash.asBytes()), Columns::pbft_blocks).empty();
}

void DbStorage::addPbftBlockToBatch(
    const taraxa::PbftBlock& pbft_block,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::pbft_blocks,
            toSlice(pbft_block.getBlockHash().asBytes()),
            pbft_block.getJsonStr());
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

bytes DbStorage::getVote(blk_hash_t const& hash) {
  return asBytes(lookup(toSlice(hash.asBytes()), Columns::votes));
}

void DbStorage::saveVote(blk_hash_t const& hash, bytes& value) {
  insert(Columns::votes, toSlice(hash.asBytes()), toSlice(value));
}

void DbStorage::addPbftCertVotesToBatch(
    const taraxa::blk_hash_t& pbft_block_hash,
    const std::vector<Vote>& cert_votes,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  RLPStream s;
  s.appendList(cert_votes.size());
  for (auto const& v : cert_votes) {
    s.append(v.rlp());
  }
  auto ss = s.out();
  batch_put(write_batch, Columns::votes, toSlice(pbft_block_hash.asBytes()),
            toSlice(ss));
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

vector<blk_hash_t> DbStorage::getOrderedDagBlocks() {
  uint64_t period = 1;
  vector<blk_hash_t> res;
  while (true) {
    auto pbft_block_hash = getPeriodPbftBlock(period);
    if (pbft_block_hash) {
      auto pbft_block = getPbftBlock(*pbft_block_hash);
      if (pbft_block) {
        for (auto const dag_block_hash :
             pbft_block->getSchedule().dag_blks_order) {
          res.push_back(dag_block_hash);
        }
      }
      period++;
      continue;
    }
    break;
  }
  return res;
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

}  // namespace taraxa
