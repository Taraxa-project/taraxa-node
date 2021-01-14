#ifndef TARAXA_NODE_DB_STORAGE
#define TARAXA_NODE_DB_STORAGE

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/write_batch.h>

#include <filesystem>
#include <functional>
#include <string_view>

#include "dag_block.hpp"
#include "logger/log.hpp"
#include "pbft_chain.hpp"
#include "transaction_status.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = std::filesystem;
enum StatusDbField : uint8_t {
  ExecutedBlkCount = 0,
  ExecutedTrxCount,
  TrxCount,
  DagBlkCount,
  DagEdgeCount,
  DbMajorVersion,
  DbMinorVersion
};
enum PbftMrgField : uint8_t { PbftRound = 0, PbftStep };

class DbException : public exception {
 public:
  explicit DbException(string const& desc) : desc_(desc) {}
  virtual ~DbException() {}
  virtual const char* what() const noexcept { return desc_.c_str(); }

 private:
  string desc_;
};

struct DbStorage {
  class Batch;
  friend class Batch;

  using Slice = rocksdb::Slice;
  using OnEntry = function<bool(Slice const&, Slice const&)>;

  struct Column {
    string const name;
    size_t const ordinal;
  };

  class Columns {
    static inline vector<Column> all_;

   public:
    static inline auto const& all = all_;

    static inline auto const default_column = all_.emplace_back(Column{kDefaultColumnFamilyName, all_.size()});

#define COLUMN(__name__) static inline auto const __name__ = all_.emplace_back(Column{#__name__, all_.size()})

    COLUMN(dag_blocks);
    COLUMN(dag_blocks_index);
    COLUMN(dag_blocks_state);
    // anchor_hash->[...dag_block_hashes_since_previous_anchor, anchor_hash]
    COLUMN(dag_finalized_blocks);
    COLUMN(transactions);
    // hash->dummy_short_value
    COLUMN(executed_transactions);
    COLUMN(trx_status);
    COLUMN(status);
    COLUMN(pbft_mgr);
    COLUMN(pbft_head);
    COLUMN(pbft_blocks);
    COLUMN(votes);
    COLUMN(period_pbft_block);
    COLUMN(dag_block_period);
    COLUMN(replay_protection);
    COLUMN(pending_transactions);
    COLUMN(final_chain_blocks);
    COLUMN(final_chain_block_details);
    COLUMN(final_chain_block_number_to_hash);
    COLUMN(final_chain_transaction_locations);
    COLUMN(final_chain_log_blooms);
    COLUMN(final_chain_receipts);
    COLUMN(final_chain_log_blooms_index);

#undef COLUMN
  };

 private:
  std::weak_ptr<DbStorage> weak_this_;
  fs::path path_;
  fs::path db_path_;
  fs::path state_db_path_;
  const std::string db_dir = "db";
  const std::string state_db_dir = "state_db";
  DB* db_;
  vector<ColumnFamilyHandle*> handles_;
  ReadOptions read_options_;
  WriteOptions write_options_;
  mutex dag_blocks_mutex_;
  atomic<uint64_t> dag_blocks_count_;
  atomic<uint64_t> dag_edge_count_;
  uint32_t db_snapshot_each_n_pbft_block_ = 0;
  uint32_t db_max_snapshots_ = 0;
  std::set<uint64_t> snapshots_;
  addr_t node_addr_;
  bool minor_version_changed_ = false;

  auto handle(Column const& col) const { return handles_[col.ordinal]; }
  static void checkStatus(rocksdb::Status const& status);

  LOG_OBJECTS_DEFINE;

 public:
  DbStorage(DbStorage const&) = delete;
  DbStorage& operator=(DbStorage const&) = delete;

 private:
  DbStorage(fs::path const& base_path, uint32_t db_snapshot_each_n_pbft_block = 0, uint32_t db_max_snapshots = 0,
            uint32_t db_revert_to_period = 0, addr_t node_addr = addr_t(), bool rebuild = 0);

 public:
  template <typename... ConstructorParams>
  static auto make(ConstructorParams&&... ctor_params) {
    // make_shared won't work in this pattern, since the constructors are private
    auto ret = s_ptr(new DbStorage(std::forward<ConstructorParams>(ctor_params)...));
    ret->weak_this_ = ret;
    return ret;
  }

  ~DbStorage();

  auto const& path() const { return path_; }
  auto dbStoragePath() const { return db_path_; }
  auto stateDbStoragePath() const { return state_db_path_; }
  Batch createWriteBatch();
  bool createSnapshot(uint64_t const& period);
  void deleteSnapshot(uint64_t const& period);
  void recoverToPeriod(uint64_t const& period);
  void loadSnapshots();

  // DAG
  dev::bytes getDagBlockRaw(blk_hash_t const& hash);
  shared_ptr<DagBlock> getDagBlock(blk_hash_t const& hash);
  string getBlocksByLevel(level_t level);
  std::vector<std::shared_ptr<DagBlock>> getDagBlocksAtLevel(level_t level, int number_of_levels);

  // DAG state
  std::map<blk_hash_t, bool> getAllDagBlockState();

  // Transaction
  void saveTransaction(Transaction const& trx);
  dev::bytes getTransactionRaw(trx_hash_t const& hash);
  shared_ptr<Transaction> getTransaction(trx_hash_t const& hash);
  shared_ptr<pair<Transaction, taraxa::bytes>> getTransactionExt(trx_hash_t const& hash);
  bool transactionInDb(trx_hash_t const& hash);

  void saveTransactionStatus(trx_hash_t const& trx, TransactionStatus const& status);
  TransactionStatus getTransactionStatus(trx_hash_t const& hash);
  std::map<trx_hash_t, TransactionStatus> getAllTransactionStatus();

  // PBFT manager
  uint64_t getPbftMgrField(PbftMrgField const& field);
  void savePbftMgrField(PbftMrgField const& field, uint64_t const& value);

  // pbft_blocks
  shared_ptr<PbftBlock> getPbftBlock(blk_hash_t const& hash);
  bool pbftBlockInDb(blk_hash_t const& hash);
  // pbft_blocks (head)
  // TODO: I would recommend storing this differently and not in the same db as
  // regular blocks with real hashes. Need remove from DB
  string getPbftHead(blk_hash_t const& hash);
  void savePbftHead(blk_hash_t const& hash, string const& pbft_chain_head_str);
  // status
  uint64_t getStatusField(StatusDbField const& field);
  void saveStatusField(StatusDbField const& field,
                       uint64_t const& value);  // unit test
  // votes
  bytes getVotes(blk_hash_t const& hash);
  // period_pbft_block
  shared_ptr<blk_hash_t> getPeriodPbftBlock(uint64_t const& period);
  // dag_block_period
  shared_ptr<uint64_t> getDagBlockPeriod(blk_hash_t const& hash);

  uint64_t getDagBlocksCount() const { return dag_blocks_count_.load(); }
  uint64_t getDagEdgeCount() const { return dag_edge_count_.load(); }

  auto getNumTransactionExecuted() { return getStatusField(StatusDbField::ExecutedTrxCount); }
  auto getNumTransactionInDag() { return getStatusField(StatusDbField::TrxCount); }
  auto getNumBlockExecuted() { return getStatusField(StatusDbField::ExecutedBlkCount); }
  uint64_t getNumDagBlocks() { return getDagBlocksCount(); }

  vector<blk_hash_t> getFinalizedDagBlockHashesByAnchor(blk_hash_t const& anchor);

  template <typename K, typename V>
  void insert(Column const& col, K const& k, V const& v) {
    checkStatus(db_->Put(write_options_, handle(col), toSlice(k), toSlice(v)));
  }

  void forEach(Column const& col, OnEntry const& f);

  bool hasMinorVersionChanged() { return minor_version_changed_; }

  inline static bytes asBytes(string const& b) {
    return bytes((byte const*)b.data(), (byte const*)(b.data() + b.size()));
  }

  inline static Slice toSlice(dev::bytesConstRef const& b) {
    return Slice(reinterpret_cast<char const*>(&b[0]), b.size());
  }

  template <unsigned N>
  inline static Slice toSlice(dev::FixedHash<N> const& h) {
    return {reinterpret_cast<char const*>(h.data()), N};
  }

  inline static Slice toSlice(dev::bytes const& b) { return toSlice(&b); }

  template <class N, typename = enable_if_t<is_arithmetic<N>::value>>
  inline static Slice toSlice(N const& n) {
    return Slice(reinterpret_cast<char const*>(&n), sizeof(N));
  }

  inline static Slice toSlice(string const& str) { return Slice(str.data(), str.size()); }

  inline static auto const& toSlice(Slice const& s) { return s; }

  template <typename T>
  inline static auto toSlices(std::vector<T> const& keys) {
    std::vector<Slice> ret;
    ret.reserve(keys.size());
    for (auto const& k : keys) {
      ret.emplace_back(toSlice(k));
    }
    return ret;
  }

  inline static auto const& toSlices(std::vector<Slice> const& ss) { return ss; }

  template <typename K>
  std::string lookup(K const& key, Column const& column) {
    std::string value;
    auto status = db_->Get(read_options_, handle(column), toSlice(key), &value);
    if (status.IsNotFound()) {
      return value;
    }
    checkStatus(status);
    return value;
  }

  class MultiGetQuery {
    shared_ptr<DbStorage> const db_;
    vector<ColumnFamilyHandle*> cfs_;
    vector<Slice> keys_;
    vector<string> str_pool_;

   public:
    explicit MultiGetQuery(shared_ptr<DbStorage> const& db, uint capacity = 0);

    template <typename T>
    MultiGetQuery& append(Column const& col, vector<T> const& keys, bool copy_key = true) {
      auto h = db_->handle(col);
      for (auto const& k : keys) {
        cfs_.emplace_back(h);
        if (copy_key) {
          auto const& slice = toSlice(k);
          keys_.emplace_back(toSlice(str_pool_.emplace_back(slice.data(), slice.size())));
        } else {
          keys_.emplace_back(toSlice(k));
        }
      }
      return *this;
    }

    template <typename T>
    MultiGetQuery& append(Column const& col, T const& key, bool copy_key = true) {
      return append(col, vector{toSlice(key)}, copy_key);
    }

    dev::bytesConstRef get_key(uint pos);
    uint size();
    vector<string> execute(bool and_reset = true);
    MultiGetQuery& reset();
  };

  class Batch {
    friend class DbStorage;

    rocksdb::WriteBatch b_;
    shared_ptr<DbStorage> db_;
    Batch(decltype(db_) db) : db_(std::move(db)) {}

   public:
    // not copyable. copying a write batch is almost always indicative of a programmer mistake
    Batch(Batch const&) = delete;
    Batch& operator=(Batch const&) = delete;
    // movable
    Batch(Batch&&) = default;
    Batch& operator=(Batch&&) = default;

    template <typename K, typename V>
    void put(Column const& col, K const& k, V const& v) {
      checkStatus(b_.Put(db_->handle(col), toSlice(k), toSlice(v)));
    }

    template <typename K>
    void remove(Column const& col, K const& k) {
      checkStatus(b_.Delete(db_->handle(col), toSlice(k)));
    }

    void commit();

    void saveDagBlock(DagBlock const& blk);
    void addDagBlockState(blk_hash_t const& blk_hash, bool finalized);
    void removeDagBlockState(blk_hash_t const& blk_hash);
    void addTransaction(Transaction const& trx);
    void addTransactionStatus(trx_hash_t const& trx, TransactionStatus const& status);
    void addPbftMgrField(PbftMrgField const& field, uint64_t const& value);
    void addPbftBlock(PbftBlock const& pbft_block);
    void addPbftHead(taraxa::blk_hash_t const& head_hash, std::string const& head_str);
    void addStatusField(StatusDbField const& field, uint64_t const& value);
    void addPbftCertVotes(taraxa::blk_hash_t const& pbft_block_hash, std::vector<Vote> const& cert_votes);
    void putFinalizedDagBlockHashesByAnchor(blk_hash_t const& anchor, vector<blk_hash_t> const& hs);
    void addPbftBlockPeriod(uint64_t const& period, taraxa::blk_hash_t const& pbft_block_hash);
    void addDagBlockPeriod(blk_hash_t const& hash, uint64_t const& period);
  };
};
}  // namespace taraxa
#endif
