#include "db_storage.hpp"
#include "transaction.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = boost::filesystem;

DbStorage::DbStorage(fs::path const& base_path, h256 const& genesis_hash,
                     bool drop_existing) {
  auto path = base_path;
  path /= fs::path(toHex(genesis_hash.ref().cropped(0, 4)));
  if (drop_existing) {
    fs::remove_all(path);
  }
  fs::create_directories(path);
  DEV_IGNORE_EXCEPTIONS(fs::permissions(path, fs::owner_all));

  rocksdb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 256;
  auto db = static_cast<rocksdb::DB*>(nullptr);
  auto status = DB::Open(options, path.string(), &db);

  if (status == Status::InvalidArgument()) {
    // Database might exist but it contains columns
    std::vector<ColumnFamilyDescriptor> column_families;
    column_families.push_back(ColumnFamilyDescriptor(kDefaultColumnFamilyName,
                                                     ColumnFamilyOptions()));
    for (int i = 1; i < column_end; i++) {
      column_families.push_back(
          ColumnFamilyDescriptor(column_names[i], ColumnFamilyOptions()));
    }
    status =
        DB::Open(DBOptions(), path.string(), column_families, &handles_, &db);
  } else if (status.ok()) {
    ColumnFamilyHandle* cf;
    handles_.push_back(nullptr);  // Default not used
    for (int i = 1; i < column_end; i++) {
      db->CreateColumnFamily(ColumnFamilyOptions(), column_names[i], &cf);
      handles_.push_back(cf);
    }
  }
  if (db && status.ok()) {
    db_.reset(db);
  }
  checkStatus(status);
}

void DbStorage::checkStatus(rocksdb::Status& _status) {
  if (_status.ok()) return;
  throw DbException((string("Db error. Status code: ") +
                     to_string(_status.code()) +
                     " SubCode: " + to_string(_status.subcode()) +
                     " Message:" + _status.ToString())
                        .c_str());
}

std::unique_ptr<WriteBatch> DbStorage::createWriteBatch() {
  return std::unique_ptr<WriteBatch>(new WriteBatch());
}

std::string DbStorage::lookup(Slice _key, Columns column) {
  std::string value;
  Status status = db_->Get(read_options_, handles_[column], _key, &value);
  if (status.IsNotFound()) return std::string();

  checkStatus(status);
  return value;
}

void DbStorage::commitWriteBatch(std::unique_ptr<WriteBatch>& write_batch) {
  auto status = db_->Write(write_options_, write_batch.get());
  checkStatus(status);
}

std::shared_ptr<DagBlock> DbStorage::getDagBlock(blk_hash_t const& hash) {
  auto blk_bytes =
      asBytes(lookup(toSlice(hash.asBytes()), Columns::dag_blocks));
  if (blk_bytes.size() > 0) {
    return std::make_shared<DagBlock>(blk_bytes);
  }
  return nullptr;
}

std::string DbStorage::getBlocksByLevel(level_t level) {
  return lookup(toSlice(level), Columns::dag_blocks_index);
}

void DbStorage::saveDagBlock(DagBlock const& blk) {
  auto write_batch = std::unique_ptr<WriteBatch>(new WriteBatch());
  auto block_bytes = blk.rlp(true);
  auto block_hash = blk.getHash();
  auto status =
      write_batch->Put(handles_[Columns::dag_blocks],
                       toSlice(block_hash.asBytes()), toSlice(block_bytes));
  checkStatus(status);
  auto level = blk.getLevel();
  std::string blocks = getBlocksByLevel(level);
  if (blocks == "") {
    blocks = blk.getHash().toString();
  } else {
    blocks = blocks + "," + blk.getHash().toString();
  }
  status = write_batch->Put(handles_[Columns::dag_blocks_index], toSlice(level),
                            toSlice(blocks));
  checkStatus(status);
  status = db_->Write(write_options_, write_batch.get());
  checkStatus(status);
}

void DbStorage::saveTransaction(Transaction const& trx)  {
  auto status = db_->Put(write_options_, handles_[Columns::transactions],
                         toSlice(trx.getHash().asBytes()), toSlice(trx.rlp(trx.hasSig())));
  checkStatus(status);
}
  

void DbStorage::saveTransactionToBlock(trx_hash_t const& trx_hash,
                                       blk_hash_t const& blk_hash) {
  auto status = db_->Put(write_options_, handles_[Columns::trx_to_blk],
                         trx_hash.toString(), blk_hash.toString());
  checkStatus(status);
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

std::shared_ptr<Transaction> DbStorage::getTransaction(trx_hash_t const& hash) {
   auto trx_bytes =
      asBytes(lookup(toSlice(hash.asBytes()), Columns::transactions));
  if (trx_bytes.size() > 0) {
    return std::make_shared<Transaction>(trx_bytes);
  }
  return nullptr;
}

std::shared_ptr<std::pair<Transaction, taraxa::bytes>> DbStorage::getTransactionExt(trx_hash_t const& hash) {
   auto trx_bytes =
      asBytes(lookup(toSlice(hash.asBytes()), Columns::transactions));
  if (trx_bytes.size() > 0) {
    return std::make_shared<std::pair<Transaction, taraxa::bytes>>(trx_bytes, trx_bytes);
  }
  return nullptr;
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
  auto status = db_->Put(write_options_, handles_[Columns::status],
                         toSlice((uint8_t)field), toSlice((uint8_t)value));
  checkStatus(status);
}

}  // namespace taraxa
