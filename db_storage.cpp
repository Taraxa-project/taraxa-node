#include "db_storage.hpp"

#include "transaction.hpp"
#include "vote.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = boost::filesystem;

std::unique_ptr<DbStorage> DbStorage::make(fs::path const& base_path,
                                           h256 const& genesis_hash,
                                           bool drop_existing) {
  auto path = base_path;
  path /= fs::path(toHex(genesis_hash.ref().cropped(0, 4)));
  if (drop_existing) {
    fs::remove_all(path);
  }
  fs::create_directories(path);
  DEV_IGNORE_EXCEPTIONS(fs::permissions(path, fs::owner_all));
  rocksdb::Options options;
  options.create_missing_column_families = true;
  options.create_if_missing = true;
  options.max_open_files = 256;
  DB* db_ = nullptr;
  vector<ColumnFamilyDescriptor> descriptors;
  for (auto& col : Columns::all) {
    descriptors.emplace_back(col.name, ColumnFamilyOptions());
  }
  decltype(handles_) handles(Columns::all.size());
  checkStatus(DB::Open(options, path.string(), descriptors, &handles, &db_));
  return u_ptr(new DbStorage(db_, move(handles)));
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
  commitWriteBatch(write_batch);
}

void DbStorage::saveTransaction(Transaction const& trx) {
  insert(Columns::transactions, toSlice(trx.getHash().asBytes()),
         toSlice(trx.rlp(trx.hasSig())));
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
            toSlice(trx.getHash().asBytes()), toSlice(trx.rlp(trx.hasSig())));
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

string DbStorage::getPbftBlockGenesis(string const& hash) {
  return lookup(hash, Columns::pbft_blocks);
}

void DbStorage::savePbftBlockGenesis(string const& hash, string const& block) {
  insert(Columns::pbft_blocks, hash, block);
}

void DbStorage::addPbftChainHeadToBatch(
    taraxa::blk_hash_t const& pbft_chain_head_hash,
    std::string const& pbft_chain_head_str,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::pbft_blocks,
            toSlice(pbft_chain_head_hash.asBytes()), pbft_chain_head_str);
}

std::shared_ptr<blk_hash_t> DbStorage::getPbftBlockOrder(
    uint64_t const& index) {
  auto block = lookup(toSlice(index), Columns::pbft_blocks_order);
  if (!block.empty()) {
    return std::make_shared<blk_hash_t>(block);
  }
  return nullptr;
}

void DbStorage::savePbftBlockOrder(uint64_t const& index,
                                   blk_hash_t const& hash) {
  insert(Columns::pbft_blocks_order, toSlice(index), hash.toString());
}

void DbStorage::addPbftBlockIndexToBatch(
    const uint64_t& pbft_block_index, const taraxa::blk_hash_t& pbft_block_hash,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::pbft_blocks_order, toSlice(pbft_block_index),
            pbft_block_hash.toString());
}

std::shared_ptr<blk_hash_t> DbStorage::getDagBlockOrder(string const& index) {
  auto block = lookup(index, Columns::dag_blocks_order);
  if (!block.empty()) {
    return std::make_shared<blk_hash_t>(block);
  }
  return nullptr;
}

void DbStorage::saveDagBlockOrder(string const& index, blk_hash_t const& hash) {
  insert(Columns::dag_blocks_order, index, hash.toString());
}

string DbStorage::getDagBlockHeight(blk_hash_t const& hash) {
  return lookup(hash.toString(), Columns::dag_blocks_height);
}

void DbStorage::saveDagBlockHeight(blk_hash_t const& hash,
                                   string const& height) {
  insert(Columns::dag_blocks_height, hash.toString(), height);
}

void DbStorage::addDagBlockOrderAndHeightToBatch(
    taraxa::blk_hash_t const& dag_block_hash,
    uint64_t const& max_dag_blocks_height,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  // Add DAG block hash into DAG blocks order DB batch.
  // DAG genesis at index 1
  batch_put(write_batch, Columns::dag_blocks_order,
            std::to_string(max_dag_blocks_height), dag_block_hash.toString());
  // Add DAG block hash into DAG blocks height DB batch
  // key : dag block hash, value : dag block height
  // DAG genesis is block height 1
  batch_put(write_batch, Columns::dag_blocks_height, dag_block_hash.toString(),
            toSlice(max_dag_blocks_height));
}

string DbStorage::getSortitionAccount(string const& key) {
  return lookup(key, Columns::sortition_accounts);
}

PbftSortitionAccount DbStorage::getSortitionAccount(addr_t const& account) {
  return PbftSortitionAccount(
      lookup(account.toString(), Columns::sortition_accounts));
}

bool DbStorage::sortitionAccountInDb(string const& key) {
  return !lookup(key, Columns::sortition_accounts).empty();
}

bool DbStorage::sortitionAccountInDb(addr_t const& account) {
  return !lookup(account.toString(), Columns::sortition_accounts).empty();
}

void DbStorage::removeSortitionAccount(addr_t const& account) {
  remove(account.toString(), Columns::sortition_accounts);
}

void DbStorage::forEachSortitionAccount(OnEntry const& f) {
  forEach(Columns::sortition_accounts, f);
}

void DbStorage::addSortitionAccountToBatch(addr_t const& address,
                                           PbftSortitionAccount& account,
                                           BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::sortition_accounts,
            address.toString(), account.getJsonStr());
}

void DbStorage::addSortitionAccountToBatch(string const& address,
                                           string& account,
                                           BatchPtr const& write_batch) {
  batch_put(write_batch, DbStorage::Columns::sortition_accounts, address,
            account);
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

shared_ptr<blk_hash_t> DbStorage::getPeriodScheduleBlock(
    uint64_t const& period) {
  auto hash = asBytes(lookup(toSlice(period), Columns::period_schedule_block));
  if (hash.size() > 0) {
    return make_shared<blk_hash_t>(hash);
  }
  return nullptr;
}

void DbStorage::addPbftBlockPeriodToBatch(
    uint64_t const& period, taraxa::blk_hash_t const& pbft_block_hash,
    const taraxa::DbStorage::BatchPtr& write_batch) {
  batch_put(write_batch, Columns::period_schedule_block, toSlice(period),
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
