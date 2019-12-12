#ifndef TARAXA_NODE_DB_STORAGE
#define TARAXA_NODE_DB_STORAGE

#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <boost/filesystem.hpp>
#include "dag_block.hpp"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "util.hpp"
#include "pbft_chain.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = boost::filesystem;
enum StatusDbField : uint8_t {
  ExecutedBlkCount = 0,
  ExecutedTrxCount,
  TrxCount
};

class DbException : public exception {
 public:
  DbException(string desc) : desc_(desc) {}
  virtual ~DbException() {}
  virtual const char* what() const noexcept { return desc_.c_str(); }

 private:
  string desc_;
};

class DbStorage {
 public:
  enum Columns : unsigned char {
    default_column = 0,
    dag_blocks,
    dag_blocks_index,
    transactions,
    trx_to_blk,
    status,
    pbft_blocks,
    pbft_blocks_order,
    dag_blocks_order,
    dag_blocks_height,
    column_end
  };

  static const constexpr char* const column_names[] = {
      "default",          "dag_blocks",        "dag_blocks_index",
      "transactions",     "trx_to_blk",        "status",
      "pbft_blocks",      "pbft_blocks_order", "dag_blocks_order",
      "dag_blocks_height"};

  DbStorage(fs::path const& base_path, h256 const& genesis_hash,
            bool drop_existing);

  ~DbStorage() {
    for (auto handle : handles_) delete handle;
  }
  void checkStatus(rocksdb::Status& _status);
  std::string lookup(Slice _key, Columns column);
  std::unique_ptr<WriteBatch> createWriteBatch();
  void commitWriteBatch(std::unique_ptr<WriteBatch>& write_batch);
  ColumnFamilyHandle* getHandle(Columns column) {
    if (handles_.size() > 0) return handles_[column];
    return NULL;
  }

  void saveDagBlock(DagBlock const& blk);
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const& hash);
  std::string getBlocksByLevel(level_t level);

  void saveTransaction(Transaction const& trx);
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const& hash);
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> getTransactionExt(
      trx_hash_t const& hash);
  bool transactionInDb(trx_hash_t const& hash);

  void saveTransactionToBlock(trx_hash_t const& trx, blk_hash_t const& hash);
  std::shared_ptr<blk_hash_t> getTransactionToBlock(trx_hash_t const& hash);
  bool transactionToBlockInDb(trx_hash_t const& hash);

  void savePbftBlock(PbftBlock const& block);
  //I would recommend storing this differently and not in the same db as regular blocks with real hashes
  void savePbftBlockGenesis(string const& hash, string const& block);
  string getPbftBlockGenesis(string const& hash);
  std::shared_ptr<PbftBlock> getPbftBlock(blk_hash_t const& hash);
  bool pbftBlockInDb(blk_hash_t const& hash);

  void savePbftBlockOrder(string const& index, blk_hash_t const& hash);
  std::shared_ptr<blk_hash_t> getPbftBlockOrder(string const& index);
  
  void saveDagBlockOrder(string const& index, blk_hash_t const& hash);
  std::shared_ptr<blk_hash_t> getDagBlockOrder(string const& index);
  
  void saveDagBlockHeight(blk_hash_t const& hash, string const& height);
  string getDagBlockHeight(blk_hash_t const& hash);
  
  uint64_t getStatusField(StatusDbField const& field);
  void saveStatusField(StatusDbField const& field, uint64_t const& value);

  inline Slice toSlice(dev::bytes const& _b) const {
    return Slice(reinterpret_cast<char const*>(&_b[0]), _b.size());
  }

  template <class N, typename = std::enable_if_t<std::is_arithmetic<N>::value>>
  inline Slice toSlice(N const& n) {
    return Slice(reinterpret_cast<char const*>(&n), sizeof(N));
  }

  inline Slice toSlice(string const& _str) {
    return Slice(_str.data(), _str.size());
  }

  inline bytes asBytes(std::string const& _b) {
    return bytes((byte const*)_b.data(), (byte const*)(_b.data() + _b.size()));
  }

 private:
  std::unique_ptr<DB> db_;
  std::vector<ColumnFamilyHandle*> handles_;
  ReadOptions read_options_;
  WriteOptions write_options_;
};
}  // namespace taraxa
#endif
