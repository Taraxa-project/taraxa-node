#ifndef TARAXA_NODE_DB_STORAGE
#define TARAXA_NODE_DB_STORAGE

#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <boost/filesystem.hpp>
#include "dag_block.hpp"
#include "pbft_chain.hpp"
#include "pbft_sortition_account.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "util.hpp"

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
    sortition_accounts,
    votes,
    period_schedule_block,
    dag_block_period,
    column_end
  };

  static const constexpr char* const column_names[] = {"default",
                                                       "dag_blocks",
                                                       "dag_blocks_index",
                                                       "transactions",
                                                       "trx_to_blk",
                                                       "status",
                                                       "pbft_blocks",
                                                       "pbft_blocks_order",
                                                       "dag_blocks_order",
                                                       "dag_blocks_height",
                                                       "sortition_accounts",
                                                       "votes",
                                                       "period_schedule_block",
                                                       "dag_block_period"};

  DbStorage(fs::path const& base_path, h256 const& genesis_hash,
            bool drop_existing);

  ~DbStorage() {
    for (auto handle : handles_) delete handle;
  }

  std::unique_ptr<WriteBatch> createWriteBatch();
  void commitWriteBatch(std::unique_ptr<WriteBatch>& write_batch);

  void saveDagBlock(DagBlock const& blk);
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const& hash);
  std::string getBlocksByLevel(level_t level);

  void saveTransaction(Transaction const& trx);
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const& hash);
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> getTransactionExt(
      trx_hash_t const& hash);
  bool transactionInDb(trx_hash_t const& hash);
  void addTransactionToBatch(Transaction const& trx,
                             std::unique_ptr<WriteBatch> const& write_batch);

  void saveTransactionToBlock(trx_hash_t const& trx, blk_hash_t const& hash);
  std::shared_ptr<blk_hash_t> getTransactionToBlock(trx_hash_t const& hash);
  bool transactionToBlockInDb(trx_hash_t const& hash);

  void savePbftBlock(PbftBlock const& block);
  // I would recommend storing this differently and not in the same db as
  // regular blocks with real hashes
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
  void addStatusFieldToBatch(StatusDbField const& field, uint64_t const& value,
                             std::unique_ptr<WriteBatch> const& write_batch);

  // Avoid using different data in same db here as well
  string getSortitionAccount(string const& key);
  PbftSortitionAccount getSortitionAccount(addr_t const& account);
  bool sortitionAccountInDb(string const& key);
  bool sortitionAccountInDb(addr_t const& account);
  void removeSortitionAccount(addr_t const& account);
  void forEachSortitionAccount(std::function<bool(Slice, Slice)> f);
  void addSortitionAccountToBatch(
      addr_t const& address, PbftSortitionAccount& account,
      std::unique_ptr<WriteBatch> const& write_batch);
  void addSortitionAccountToBatch(
      string const& address, string& account,
      std::unique_ptr<WriteBatch> const& write_batch);

  bytes getVote(blk_hash_t const& hash);
  void saveVote(blk_hash_t const& hash, bytes& value);

  shared_ptr<blk_hash_t> getPeriodScheduleBlock(uint64_t const& period);
  void savePeriodScheduleBlock(uint64_t const& period, blk_hash_t const& hash);

  std::shared_ptr<uint64_t> getDagBlockPeriod(blk_hash_t const& hash);
  void saveDagBlockPeriod(blk_hash_t const& hash, uint64_t const& period);
  void addDagBlockPeriodToBatch(blk_hash_t const& hash, uint64_t const& period,
                                std::unique_ptr<WriteBatch> const& write_batch);

 private:
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

  void checkStatus(rocksdb::Status& _status);
  std::string lookup(Slice _key, Columns column);
  void remove(Slice _key, Columns column);
  ColumnFamilyHandle* getHandle(Columns column) {
    if (handles_.size() > 0) return handles_[column];
    return NULL;
  }

  std::unique_ptr<DB> db_;
  std::vector<ColumnFamilyHandle*> handles_;
  ReadOptions read_options_;
  WriteOptions write_options_;
};
}  // namespace taraxa
#endif
