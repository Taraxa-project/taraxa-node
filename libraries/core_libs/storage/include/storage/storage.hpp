#pragma once

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/write_batch.h>

#include <filesystem>
#include <functional>
#include <string_view>

#include "common/types.hpp"
#include "dag/dag_block.hpp"
#include "logger/logger.hpp"
#include "pbft/pbft_block.hpp"
#include "pbft/period_data.hpp"
#include "storage/uint_comparator.hpp"
#include "transaction/transaction.hpp"
#include "vote_manager/verified_votes.hpp"

namespace taraxa {
namespace fs = std::filesystem;
struct SortitionParamsChange;

enum StatusDbField : uint8_t {
  ExecutedBlkCount = 0,
  ExecutedTrxCount,
  TrxCount,
  DagBlkCount,
  DagEdgeCount,
  DbMajorVersion,
  DbMinorVersion
};

enum class PbftMgrField : uint8_t { Round = 0, Step };

enum PbftMgrStatus : uint8_t {
  ExecutedBlock = 0,
  ExecutedInRound,
  NextVotedSoftValue,
  NextVotedNullBlockHash,
};

class DbException : public std::exception {
 public:
  explicit DbException(const std::string& desc) : desc_(desc) {}
  virtual ~DbException() = default;

  DbException(const DbException&) = default;
  DbException(DbException&&) = default;
  DbException& operator=(const DbException&) = delete;
  DbException& operator=(DbException&&) = delete;

  virtual const char* what() const noexcept { return desc_.c_str(); }

 private:
  const std::string desc_;
};

class DbStorage : public std::enable_shared_from_this<DbStorage> {
 public:
  using Slice = rocksdb::Slice;
  using Batch = rocksdb::WriteBatch;
  using OnEntry = std::function<void(Slice const&, Slice const&)>;

  class Column {
    string const name_;

   public:
    size_t const ordinal_;
    const rocksdb::Comparator* comparator_;

    Column(string name, size_t ordinal, const rocksdb::Comparator* comparator)
        : name_(std::move(name)), ordinal_(ordinal), comparator_(comparator) {}

    Column(string name, size_t ordinal) : name_(std::move(name)), ordinal_(ordinal), comparator_(nullptr) {}

    auto const& name() const { return ordinal_ ? name_ : rocksdb::kDefaultColumnFamilyName; }
  };

  class Columns {
    static inline std::vector<Column> all_;

   public:
    static inline auto const& all = all_;

#define COLUMN(__name__) static inline auto const __name__ = all_.emplace_back(#__name__, all_.size())
#define COLUMN_W_COMP(__name__, ...) \
  static inline auto const __name__ = all_.emplace_back(#__name__, all_.size(), __VA_ARGS__)

    // do not change/move
    COLUMN(default_column);
    // Contains full data for an executed PBFT block including PBFT block, cert votes, dag blocks and transactions
    COLUMN_W_COMP(period_data, getIntComparator<PbftPeriod>());
    COLUMN(genesis);
    COLUMN(dag_blocks);
    COLUMN(dag_blocks_index);
    COLUMN(transactions);
    COLUMN(trx_period);
    COLUMN(status);
    COLUMN(pbft_mgr_round_step);
    COLUMN(pbft_mgr_status);
    COLUMN(cert_voted_block_in_round);  // Cert voted block + round -> node voted for this block
    COLUMN(proposed_pbft_blocks);       // Proposed pbft blocks
    COLUMN(pbft_head);
    COLUMN(latest_round_own_votes);             // own votes of any type for the latest round
    COLUMN(latest_round_two_t_plus_one_votes);  // 2t+1 votes bundles of any type for the latest round
    COLUMN(latest_reward_votes);                // latest reward votes - cert votes for the latest finalized block
    // TODO: we are saving here only our own votes so we should either fix it or rename the column
    // TODO: we can save here all of our own votes + bundles of 2t+1 soft votes & next votes for previous round
    //       we would keep last 2 rounds here -> on new period everything id removed, on new round previous round is
    //       removed
    COLUMN(verified_votes);
    // TODO: can probably delete
    COLUMN(next_votes);  // only for previous PBFT round
    // TODO: can probably delete
    COLUMN(last_block_cert_votes);  // cert votes for last block in pbft chain
    COLUMN(pbft_block_period);
    COLUMN(dag_block_period);
    COLUMN_W_COMP(proposal_period_levels_map, getIntComparator<uint64_t>());
    COLUMN(final_chain_meta);
    COLUMN(final_chain_transaction_location_by_hash);
    COLUMN(final_chain_replay_protection);
    COLUMN(final_chain_transaction_hashes_by_blk_number);
    COLUMN(final_chain_transaction_count_by_blk_number);
    COLUMN(final_chain_blk_by_number);
    COLUMN(final_chain_blk_hash_by_number);
    COLUMN(final_chain_blk_number_by_hash);
    COLUMN(final_chain_receipt_by_trx_hash);
    COLUMN(final_chain_log_blooms_index);
    COLUMN_W_COMP(sortition_params_change, getIntComparator<PbftPeriod>());

#undef COLUMN
#undef COLUMN_W_COMP
  };

 private:
  fs::path path_;
  fs::path db_path_;
  fs::path state_db_path_;
  const std::string kDbDir = "db";
  const std::string kStateDbDir = "state_db";
  std::unique_ptr<rocksdb::DB> db_;
  std::vector<rocksdb::ColumnFamilyHandle*> handles_;
  rocksdb::ReadOptions read_options_;
  rocksdb::WriteOptions write_options_;
  std::mutex dag_blocks_mutex_;
  std::atomic<uint64_t> dag_blocks_count_;
  std::atomic<uint64_t> dag_edge_count_;
  const uint32_t kDbSnapshotsEachNblock = 0;
  std::atomic<bool> snapshots_enabled_ = true;
  const uint32_t kDbSnapshotsMaxCount = 0;
  std::set<PbftPeriod> snapshots_;

  bool minor_version_changed_ = false;

  auto handle(Column const& col) const { return handles_[col.ordinal_]; }

  LOG_OBJECTS_DEFINE

 public:
  explicit DbStorage(fs::path const& base_path, uint32_t db_snapshot_each_n_pbft_block = 0, uint32_t max_open_files = 0,
                     uint32_t db_max_snapshots = 0, PbftPeriod db_revert_to_period = 0, addr_t node_addr = addr_t(),
                     bool rebuild = false, bool rebuild_columns = false);
  ~DbStorage();

  DbStorage(const DbStorage&) = delete;
  DbStorage(DbStorage&&) = delete;
  DbStorage& operator=(const DbStorage&) = delete;
  DbStorage& operator=(DbStorage&&) = delete;

  auto const& path() const { return path_; }
  auto dbStoragePath() const { return db_path_; }
  auto stateDbStoragePath() const { return state_db_path_; }
  static Batch createWriteBatch();
  void commitWriteBatch(Batch& write_batch, rocksdb::WriteOptions const& opts);
  void commitWriteBatch(Batch& write_batch) { commitWriteBatch(write_batch, write_options_); }

  void rebuildColumns(const rocksdb::Options& options);
  bool createSnapshot(PbftPeriod period);
  void deleteSnapshot(PbftPeriod period);
  void recoverToPeriod(PbftPeriod period);
  void loadSnapshots();
  void disableSnapshots();
  void enableSnapshots();

  // Genesis
  void setGenesisHash(const h256& genesis_hash);
  std::optional<h256> getGenesisHash();

  // Period data
  void savePeriodData(const PeriodData& period_data, Batch& write_batch);
  void clearPeriodDataHistory(PbftPeriod period);
  dev::bytes getPeriodDataRaw(PbftPeriod period) const;
  std::optional<PbftBlock> getPbftBlock(PbftPeriod period) const;
  blk_hash_t getPeriodBlockHash(PbftPeriod period) const;
  std::optional<SharedTransactions> getPeriodTransactions(PbftPeriod period) const;

  /**
   * @brief Gets finalized transactions from provided hashes
   *
   * @param trx_hashes
   *
   * @return Returns transactions found if all transactions are present. If there is a transaction missing, no
   * transaction is returned and missing trx hash is returned
   */
  std::pair<std::optional<SharedTransactions>, trx_hash_t> getFinalizedTransactions(
      std::vector<trx_hash_t> const& trx_hashes) const;

  // DAG
  void saveDagBlock(DagBlock const& blk, Batch* write_batch_p = nullptr);
  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const& hash);
  bool dagBlockInDb(blk_hash_t const& hash);
  std::set<blk_hash_t> getBlocksByLevel(level_t level);
  std::vector<std::shared_ptr<DagBlock>> getDagBlocksAtLevel(level_t level, int number_of_levels);
  void updateDagBlockCounters(std::vector<DagBlock> blks);
  std::map<level_t, std::vector<DagBlock>> getNonfinalizedDagBlocks();
  void removeDagBlockBatch(Batch& write_batch, blk_hash_t const& hash);
  void removeDagBlock(blk_hash_t const& hash);
  // Sortition params
  void saveSortitionParamsChange(PbftPeriod period, const SortitionParamsChange& params, DbStorage::Batch& batch);
  std::deque<SortitionParamsChange> getLastSortitionParams(size_t count);
  std::optional<SortitionParamsChange> getParamsChangeForPeriod(PbftPeriod period);

  // Transaction
  void saveTransaction(Transaction const& trx);
  std::shared_ptr<Transaction> getTransaction(trx_hash_t const& hash);
  SharedTransactions getAllNonfinalizedTransactions();
  bool transactionInDb(trx_hash_t const& hash);
  bool transactionFinalized(trx_hash_t const& hash);
  std::vector<bool> transactionsInDb(std::vector<trx_hash_t> const& trx_hashes);
  std::vector<bool> transactionsFinalized(std::vector<trx_hash_t> const& trx_hashes);
  void addTransactionToBatch(Transaction const& trx, Batch& write_batch);
  void removeTransactionToBatch(trx_hash_t const& trx, Batch& write_batch);

  void saveTransactionPeriod(trx_hash_t const& trx, PbftPeriod period, uint32_t position);
  void addTransactionPeriodToBatch(Batch& write_batch, trx_hash_t const& trx, PbftPeriod period, uint32_t position);
  std::optional<std::pair<PbftPeriod, uint32_t>> getTransactionPeriod(trx_hash_t const& hash) const;
  std::unordered_map<trx_hash_t, PbftPeriod> getAllTransactionPeriod();

  // PBFT manager
  uint32_t getPbftMgrField(PbftMgrField field);
  void savePbftMgrField(PbftMgrField field, uint32_t value);
  void addPbftMgrFieldToBatch(PbftMgrField field, uint32_t value, Batch& write_batch);

  bool getPbftMgrStatus(PbftMgrStatus field);
  void savePbftMgrStatus(PbftMgrStatus field, bool const& value);
  void addPbftMgrStatusToBatch(PbftMgrStatus field, bool const& value, Batch& write_batch);

  void saveCertVotedBlockInRound(PbftRound round, const std::shared_ptr<PbftBlock>& block);
  std::optional<std::pair<PbftRound, std::shared_ptr<PbftBlock>>> getCertVotedBlockInRound() const;
  void removeCertVotedBlockInRound(Batch& write_batch);

  // pbft_blocks
  std::optional<PbftBlock> getPbftBlock(blk_hash_t const& hash);
  bool pbftBlockInDb(blk_hash_t const& hash);

  // Proposed pbft blocks
  void saveProposedPbftBlock(const std::shared_ptr<PbftBlock>& block);
  void removeProposedPbftBlock(const blk_hash_t& block_hash, Batch& write_batch);
  std::vector<std::shared_ptr<PbftBlock>> getProposedPbftBlocks();

  // pbft_blocks (head)
  string getPbftHead(blk_hash_t const& hash);
  void savePbftHead(blk_hash_t const& hash, string const& pbft_chain_head_str);
  void addPbftHeadToBatch(taraxa::blk_hash_t const& head_hash, std::string const& head_str, Batch& write_batch);

  // status
  uint64_t getStatusField(StatusDbField const& field);
  void saveStatusField(StatusDbField const& field, uint64_t value);
  void addStatusFieldToBatch(StatusDbField const& field, uint64_t value, Batch& write_batch);

  // Own votes for the latest round
  void saveOwnVerifiedVote(const std::shared_ptr<Vote>& vote);
  std::vector<std::shared_ptr<Vote>> getOwnVerifiedVotes();
  void clearOwnVerifiedVotes(Batch& write_batch);

  // 2t+1 votes bundles for the latest round
  void replaceTwoTPlusOneVotes(TwoTPlusOneVotedBlockType type, const std::vector<std::shared_ptr<Vote>>& votes);
  std::vector<std::shared_ptr<Vote>> getAllTwoTPlusOneVotes();

  // Reward votes - cert votes for the latest finalized block
  void replaceRewardVotes(const std::vector<std::shared_ptr<Vote>>& votes);
  void saveRewardVote(const std::shared_ptr<Vote>& vote);
  std::vector<std::shared_ptr<Vote>> getRewardVotes();

  // TODO: these can be deleted start
  // Verified votes
  std::vector<std::shared_ptr<Vote>> getVerifiedVotes();
  std::shared_ptr<Vote> getVerifiedVote(vote_hash_t const& vote_hash);
  void saveVerifiedVote(std::shared_ptr<Vote> const& vote);
  void addVerifiedVoteToBatch(std::shared_ptr<Vote> const& vote, Batch& write_batch);
  void removeVerifiedVoteToBatch(vote_hash_t const& vote_hash, Batch& write_batch);

  // Certified votes
  std::vector<std::shared_ptr<Vote>> getCertVotes(PbftPeriod period);

  // Next votes
  std::vector<std::shared_ptr<Vote>> getPreviousRoundNextVotes();
  void savePreviousRoundNextVotes(std::vector<std::shared_ptr<Vote>> const& next_votes);
  void removePreviousRoundNextVotes();

  // last block cert votes
  void saveLastBlockCertVote(const std::shared_ptr<Vote>& cert_vote);
  void addLastBlockCertVotesToBatch(std::vector<std::shared_ptr<Vote>> const& cert_votes,
                                    std::unordered_map<vote_hash_t, std::shared_ptr<Vote>> const& old_cert_votes,
                                    Batch& write_batch);
  std::vector<std::shared_ptr<Vote>> getLastBlockCertVotes();
  void removeLastBlockCertVotes(const vote_hash_t& hash);
  // TODO: these can be deleted end

  // period_pbft_block
  void addPbftBlockPeriodToBatch(PbftPeriod period, taraxa::blk_hash_t const& pbft_block_hash, Batch& write_batch);
  std::pair<bool, PbftPeriod> getPeriodFromPbftHash(taraxa::blk_hash_t const& pbft_block_hash);
  // dag_block_period
  std::shared_ptr<std::pair<PbftPeriod, uint32_t>> getDagBlockPeriod(blk_hash_t const& hash);
  void addDagBlockPeriodToBatch(blk_hash_t const& hash, PbftPeriod period, uint32_t position, Batch& write_batch);

  uint64_t getDagBlocksCount() const { return dag_blocks_count_.load(); }
  uint64_t getDagEdgeCount() const { return dag_edge_count_.load(); }

  auto getNumTransactionExecuted() { return getStatusField(StatusDbField::ExecutedTrxCount); }
  auto getNumTransactionInDag() { return getStatusField(StatusDbField::TrxCount); }
  auto getNumBlockExecuted() { return getStatusField(StatusDbField::ExecutedBlkCount); }
  uint64_t getNumDagBlocks() { return getDagBlocksCount(); }

  std::vector<blk_hash_t> getFinalizedDagBlockHashesByPeriod(PbftPeriod period);
  std::vector<std::shared_ptr<DagBlock>> getFinalizedDagBlockByPeriod(PbftPeriod period);

  // DPOS level to proposal period map
  std::optional<uint64_t> getProposalPeriodForDagLevel(uint64_t level);
  void saveProposalPeriodDagLevelsMap(uint64_t level, PbftPeriod period);
  void addProposalPeriodDagLevelsMapToBatch(uint64_t level, PbftPeriod period, Batch& write_batch);

  bool hasMinorVersionChanged() { return minor_version_changed_; }

  void compactColumn(Column const& column) { db_->CompactRange({}, handle(column), nullptr, nullptr); }

  inline static bytes asBytes(string const& b) {
    return bytes((byte const*)b.data(), (byte const*)(b.data() + b.size()));
  }

  template <typename T>
  inline static Slice make_slice(T const* begin, size_t size) {
    if (!size) {
      return {};
    }
    return {reinterpret_cast<char const*>(begin), size};
  }

  inline static Slice toSlice(dev::bytesConstRef const& b) { return make_slice(b.data(), b.size()); }

  template <unsigned N>
  inline static Slice toSlice(dev::FixedHash<N> const& h) {
    return make_slice(h.data(), N);
  }

  inline static Slice toSlice(dev::bytes const& b) { return make_slice(b.data(), b.size()); }

  template <class N>
  inline static auto toSlice(N const& n) -> std::enable_if_t<std::is_integral_v<N> || std::is_enum_v<N>, Slice> {
    return make_slice(&n, sizeof(N));
  }

  inline static Slice toSlice(string const& str) { return make_slice(str.data(), str.size()); }

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
  std::string lookup(K const& key, Column const& column) const {
    std::string value;
    auto status = db_->Get(read_options_, handle(column), toSlice(key), &value);
    if (status.IsNotFound()) {
      return value;
    }
    checkStatus(status);
    return value;
  }

  template <typename Int, typename K>
  auto lookup_int(K const& key, Column const& column) -> std::enable_if_t<std::is_integral_v<Int>, std::optional<Int>> {
    auto str = lookup(key, column);
    if (str.empty()) {
      return std::nullopt;
    }
    return *reinterpret_cast<Int*>(str.data());
  }

  template <typename K>
  bool exist(K const& key, Column const& column) {
    std::string value;
    // KeyMayExist can lead to a few false positives, but not false negatives.
    if (db_->KeyMayExist(read_options_, handle(column), toSlice(key), &value)) {
      auto status = db_->Get(read_options_, handle(column), toSlice(key), &value);
      if (status.IsNotFound()) {
        return false;
      }
      checkStatus(status);
      return !value.empty();
    }
    return false;
  }

  static void checkStatus(rocksdb::Status const& status);

  template <typename K, typename V>
  void insert(Column const& col, K const& k, V const& v) {
    checkStatus(db_->Put(write_options_, handle(col), toSlice(k), toSlice(v)));
  }

  template <typename K, typename V>
  void insert(Batch& batch, Column const& col, K const& k, V const& v) {
    checkStatus(batch.Put(handle(col), toSlice(k), toSlice(v)));
  }

  template <typename K>
  void remove(Column const& col, K const& k) {
    checkStatus(db_->Delete(write_options_, handle(col), toSlice(k)));
  }

  template <typename K>
  void remove(Batch& batch, Column const& col, K const& k) {
    checkStatus(batch.Delete(handle(col), toSlice(k)));
  }

  void forEach(Column const& col, OnEntry const& f);
};

using DB = DbStorage;

}  // namespace taraxa
