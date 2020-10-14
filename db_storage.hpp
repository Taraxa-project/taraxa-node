#ifndef TARAXA_NODE_DB_STORAGE
#define TARAXA_NODE_DB_STORAGE

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/write_batch.h>

#include <boost/filesystem.hpp>
#include <functional>
#include <string_view>

#include "dag_block.hpp"
#include "pbft_chain.hpp"
#include "transaction_status.hpp"
#include "util.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = boost::filesystem;
enum StatusDbField : uint8_t {
  ExecutedBlkCount = 0,
  ExecutedTrxCount,
  TrxCount,
  DagBlkCount,
  DagEdgeCount
};

class DbException : public exception {
 public:
  explicit DbException(string const& desc) : desc_(desc) {}
  virtual ~DbException() {}
  virtual const char* what() const noexcept { return desc_.c_str(); }

 private:
  string desc_;
};

struct DbStorage {
  using BatchPtr = shared_ptr<rocksdb::WriteBatch>;
  using OnEntry = function<bool(Slice const&, Slice const&)>;

  struct Column {
    string const name;
    size_t const ordinal;
  };

  class Columns {
    static inline vector<Column> all_;

   public:
    static inline auto const& all = all_;

    static inline auto const default_column =
        all_.emplace_back(Column{kDefaultColumnFamilyName, all_.size()});

#define COLUMN(__name__)              \
  static inline auto const __name__ = \
      all_.emplace_back(Column{#__name__, all_.size()})

    COLUMN(dag_blocks);
    COLUMN(dag_blocks_index);
    COLUMN(dag_blocks_state);
    COLUMN(transactions);
    COLUMN(trx_to_blk);
    COLUMN(trx_status);
    COLUMN(status);
    COLUMN(pbft_head);
    COLUMN(pbft_blocks);
    COLUMN(votes);
    COLUMN(period_pbft_block);
    COLUMN(dag_block_period);
    COLUMN(replay_protection);
    COLUMN(pending_transactions);
    COLUMN(aleth_chain);
    COLUMN(aleth_chain_extras);

#undef COLUMN
  };

 private:
  fs::path path_;
  DB* db_;
  vector<ColumnFamilyHandle*> handles_;
  ReadOptions read_options_;
  WriteOptions write_options_;
  mutex dag_blocks_mutex_;
  atomic<uint64_t> dag_blocks_count_;
  atomic<uint64_t> dag_edge_count_;

  auto handle(Column const& col) const { return handles_[col.ordinal]; }

 public:
  DbStorage(DbStorage const&) = delete;
  DbStorage& operator=(DbStorage const&) = delete;

  DbStorage(fs::path const& base_path);
  ~DbStorage();

  auto const& path() const { return path_; }
  BatchPtr createWriteBatch();
  void commitWriteBatch(BatchPtr const& write_batch);

  void batch_put(BatchPtr const& batch, Column const& col, Slice const& k,
                 Slice const& v) {
    checkStatus(batch->Put(handle(col), k, v));
  }

  void batch_delete(BatchPtr const& batch, Column const& col, Slice const& k) {
    checkStatus(batch->Delete(handle(col), k));
  }

  // DAG
  void saveDagBlock(DagBlock const& blk);
  dev::bytes getDagBlockRaw(blk_hash_t const& hash);
  shared_ptr<DagBlock> getDagBlock(blk_hash_t const& hash);
  string getBlocksByLevel(level_t level);
  std::vector<std::shared_ptr<DagBlock>> getDagBlocksAtLevel(
      level_t level, int number_of_levels);

  // DAG state
  void saveDagBlockState(blk_hash_t const& blk_hash, bool finalized);
  void addDagBlockStateToBatch(BatchPtr const& write_batch,
                               blk_hash_t const& blk_hash, bool finalized);
  void removeDagBlockStateToBatch(BatchPtr const& write_batch,
                                  blk_hash_t const& blk_hash);
  std::map<blk_hash_t, bool> getAllDagBlockState();

  // Transaction
  void saveTransaction(Transaction const& trx);
  dev::bytes getTransactionRaw(trx_hash_t const& hash);
  shared_ptr<Transaction> getTransaction(trx_hash_t const& hash);
  shared_ptr<pair<Transaction, taraxa::bytes>> getTransactionExt(
      trx_hash_t const& hash);
  bool transactionInDb(trx_hash_t const& hash);
  void addTransactionToBatch(Transaction const& trx,
                             BatchPtr const& write_batch);

  void saveTransactionToBlock(trx_hash_t const& trx, blk_hash_t const& hash);
  shared_ptr<blk_hash_t> getTransactionToBlock(trx_hash_t const& hash);
  bool transactionToBlockInDb(trx_hash_t const& hash);

  void saveTransactionStatus(trx_hash_t const& trx,
                             TransactionStatus const& status);
  void addTransactionStatusToBatch(BatchPtr const& write_batch,
                                   trx_hash_t const& trx,
                                   TransactionStatus const& status);
  TransactionStatus getTransactionStatus(trx_hash_t const& hash);
  std::map<trx_hash_t, TransactionStatus> getAllTransactionStatus();

  // pbft_blocks
  shared_ptr<PbftBlock> getPbftBlock(blk_hash_t const& hash);
  void savePbftBlock(PbftBlock const& block);  // for unit test
  bool pbftBlockInDb(blk_hash_t const& hash);
  void addPbftBlockToBatch(PbftBlock const& pbft_block,
                           BatchPtr const& write_batch);
  // pbft_blocks (head)
  // TODO: I would recommend storing this differently and not in the same db as
  // regular blocks with real hashes. Need remove from DB
  string getPbftHead(blk_hash_t const& hash);
  void savePbftHead(blk_hash_t const& hash, string const& pbft_chain_head_str);
  void addPbftHeadToBatch(taraxa::blk_hash_t const& head_hash,
                          std::string const& head_str,
                          BatchPtr const& write_batch);
  // status
  uint64_t getStatusField(StatusDbField const& field);
  void saveStatusField(StatusDbField const& field,
                       uint64_t const& value);  // unit test
  void addStatusFieldToBatch(StatusDbField const& field, uint64_t const& value,
                             BatchPtr const& write_batch);
  // votes
  bytes getVote(blk_hash_t const& hash);
  void saveVote(blk_hash_t const& hash, bytes& value);  // for unit test
  void addPbftCertVotesToBatch(taraxa::blk_hash_t const& pbft_block_hash,
                               std::vector<Vote> const& cert_votes,
                               BatchPtr const& write_batch);
  // period_pbft_block
  shared_ptr<blk_hash_t> getPeriodPbftBlock(uint64_t const& period);
  void addPbftBlockPeriodToBatch(uint64_t const& period,
                                 taraxa::blk_hash_t const& pbft_block_hash,
                                 BatchPtr const& write_batch);
  // dag_block_period
  shared_ptr<uint64_t> getDagBlockPeriod(blk_hash_t const& hash);
  void addDagBlockPeriodToBatch(blk_hash_t const& hash, uint64_t const& period,
                                BatchPtr const& write_batch);

  uint64_t getDagBlocksCount() const { return dag_blocks_count_.load(); }
  uint64_t getDagEdgeCount() const { return dag_edge_count_.load(); }

  vector<blk_hash_t> getOrderedDagBlocks();

  auto getNumTransactionExecuted() {
    return getStatusField(StatusDbField::ExecutedTrxCount);
  }
  auto getNumTransactionInDag() {
    return getStatusField(StatusDbField::TrxCount);
  }
  auto getNumBlockExecuted() {
    return getStatusField(StatusDbField::ExecutedBlkCount);
  }
  uint64_t getNumDagBlocks() { return getDagBlocksCount(); }

  string lookup(Slice key, Column const& column);
  void insert(Column const& col, Slice const& k, Slice const& v);
  void remove(Slice key, Column const& column);
  void forEach(Column const& col, OnEntry const& f);

  inline static Slice toSlice(dev::bytesConstRef const& b) {
    return Slice(reinterpret_cast<char const*>(&b[0]), b.size());
  }

  inline static Slice toSlice(dev::bytes const& b) { return toSlice(&b); }

  template <class N, typename = enable_if_t<is_arithmetic<N>::value>>
  inline static Slice toSlice(N const& n) {
    return Slice(reinterpret_cast<char const*>(&n), sizeof(N));
  }

  inline static Slice toSlice(string const& str) {
    return Slice(str.data(), str.size());
  }

  inline static bytes asBytes(string const& b) {
    return bytes((byte const*)b.data(), (byte const*)(b.data() + b.size()));
  }

  static void checkStatus(rocksdb::Status const& status);
};
}  // namespace taraxa
#endif
