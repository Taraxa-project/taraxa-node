#include "final_chain/final_chain.hpp"

#include "common/constants.hpp"
#include "common/thread_pool.hpp"
#include "final_chain/replay_protection_service.hpp"
#include "final_chain/trie_common.hpp"
#include "vote/vote.hpp"

namespace taraxa::final_chain {
using namespace std;
using namespace dev;

enum DBMetaKeys { LAST_NUMBER = 1 };

class FinalChainImpl final : public FinalChain {
  shared_ptr<DB> db_;
  unique_ptr<ReplayProtectionService> replay_protection_service_;
  StateAPI state_api_;

  mutable shared_mutex last_block_mu_;
  mutable shared_ptr<BlockHeader> last_block_;

  // It is not prepared to use more then 1 thread. Examine it if you want to change threads count
  util::ThreadPool executor_thread_{1};

  atomic<uint64_t> num_executed_dag_blk_ = 0;
  atomic<uint64_t> num_executed_trx_ = 0;

  rocksdb::WriteOptions const db_opts_w_ = [] {
    rocksdb::WriteOptions ret;
    ret.sync = true;
    return ret;
  }();

  LOG_OBJECTS_DEFINE

 public:
  FinalChainImpl(shared_ptr<DB> const& db, Config const& config, addr_t const& node_addr)
      : db_(db),
        // replay_protection_service_(NewReplayProtectionService({}, db)),
        state_api_([this](auto n) { return block_hash(n).value_or(ZeroHash()); },  //
                   config.state,
                   {
                       1500,
                       4,
                   },
                   {
                       db->stateDbStoragePath().string(),
                   }) {
    LOG_OBJECTS_CREATE("EXECUTOR");
    num_executed_dag_blk_ = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
    num_executed_trx_ = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
    auto state_db_descriptor = state_api_.get_last_committed_state_descriptor();
    if (auto last_blk_num = db_->lookup_int<EthBlockNumber>(LAST_NUMBER, DB::Columns::final_chain_meta); last_blk_num) {
      last_block_ = make_shared<BlockHeader>();
      last_block_->rlp(RLP(db_->lookup(*last_blk_num, DB::Columns::final_chain_blk_by_number)));
      if (last_block_->number != state_db_descriptor.blk_num) {
        assert(state_db_descriptor.blk_num < last_block_->number);
        for (auto n = state_db_descriptor.blk_num + 1; n <= last_block_->number; ++n) {
          auto blk = n == last_block_->number ? last_block_ : FinalChainImpl::block_header(n);
          auto res =
              state_api_.transition_state({blk->author, blk->gas_limit, blk->timestamp, BlockHeader::difficulty()},
                                          to_state_api_transactions(transactions(blk->number)));
          assert(res.state_root == blk->state_root);
          state_api_.transition_state_commit();
        }
      }
    } else {
      assert(state_db_descriptor.blk_num == 0);
      auto batch = db_->createWriteBatch();
      last_block_ = append_block(batch, config.genesis_block_fields.author, config.genesis_block_fields.timestamp,
                                 GAS_LIMIT, state_db_descriptor.state_root);
      db_->commitWriteBatch(batch, db_opts_w_);
    }
  }

  void stop() override { executor_thread_.stop(); }

  future<shared_ptr<FinalizationResult const>> finalize(SyncBlock&& new_blk,
                                                        std::vector<h256>&& finalized_dag_blk_hashes,
                                                        finalize_precommit_ext precommit_ext = {}) override {
    auto p = make_shared<promise<shared_ptr<FinalizationResult const>>>();
    executor_thread_.post([this, new_blk = std::move(new_blk),
                           finalized_dag_blk_hashes = std::move(finalized_dag_blk_hashes),
                           precommit_ext = move(precommit_ext), p]() mutable {
      p->set_value(finalize_(std::move(new_blk), std::move(finalized_dag_blk_hashes), precommit_ext));
    });
    return p->get_future();
  }

  shared_ptr<FinalizationResult> finalize_(SyncBlock&& new_blk, std::vector<h256>&& finalized_dag_blk_hashes,
                                           finalize_precommit_ext const& precommit_ext) {
    auto batch = db_->createWriteBatch();
    Transactions to_execute;
    {
      // This artificial scope will make sure we clean up the big chunk of memory allocated for this batch-processing
      // stuff as soon as possible
      DB::MultiGetQuery db_query(db_, new_blk.transactions.size());
      for (auto const& trx : new_blk.transactions) {
        db_query.append(DB::Columns::final_chain_transaction_location_by_hash, trx.getHash());
      }
      auto trx_db_results = db_query.execute(false);
      to_execute.reserve(new_blk.transactions.size());
      for (size_t i = 0; i < new_blk.transactions.size(); ++i) {
        if (auto has_been_executed = !trx_db_results[i].empty(); has_been_executed) {
          continue;
        }
        // Non-executed trxs
        auto const& trx = new_blk.transactions[i];
        if (!(replay_protection_service_ &&
              replay_protection_service_->is_nonce_stale(trx.getSender(), trx.getNonce()))) [[likely]] {
          to_execute.push_back(std::move(trx));
        }
      }
    }
    block_applying_emitter_.emit(block_header()->number + 1);
    auto const& [exec_results, state_root] = state_api_.transition_state(
        {new_blk.pbft_blk->getBeneficiary(), GAS_LIMIT, new_blk.pbft_blk->getTimestamp(), BlockHeader::difficulty()},
        to_state_api_transactions(to_execute));
    TransactionReceipts receipts;
    receipts.reserve(exec_results.size());
    gas_t cumulative_gas_used = 0;
    for (auto const& r : exec_results) {
      LogEntries logs;
      logs.reserve(r.logs.size());
      for (auto const& l : r.logs) {
        logs.emplace_back(LogEntry{l.address, l.topics, l.data});
      }
      receipts.emplace_back(TransactionReceipt{
          r.code_err.empty() && r.consensus_err.empty(),
          r.gas_used,
          cumulative_gas_used += r.gas_used,
          move(logs),
          r.new_contract_addr ? optional(r.new_contract_addr) : nullopt,
      });
    }
    auto blk_header = append_block(batch, new_blk.pbft_blk->getBeneficiary(), new_blk.pbft_blk->getTimestamp(),
                                   GAS_LIMIT, state_root, to_execute, receipts);
    if (replay_protection_service_) {
      // Update replay protection service, like nonce watermark. Nonce watermark has been disabled
      replay_protection_service_->update(
          batch, blk_header->number, util::make_range_view(to_execute).map([](auto const& trx) {
            return ReplayProtectionService::TransactionInfo{trx.getSender(), trx.getNonce()};
          }));
    }
    // Update number of executed DAG blocks and transactions
    auto num_executed_dag_blk = num_executed_dag_blk_ + finalized_dag_blk_hashes.size();
    auto num_executed_trx = num_executed_trx_ + to_execute.size();
    if (!finalized_dag_blk_hashes.empty()) {
      db_->insert(batch, DB::Columns::status, StatusDbField::ExecutedBlkCount, num_executed_dag_blk);
      db_->insert(batch, DB::Columns::status, StatusDbField::ExecutedTrxCount, num_executed_trx);
      LOG(log_nf_) << "Executed dag blocks #" << num_executed_dag_blk_ - finalized_dag_blk_hashes.size() << "-"
                   << num_executed_dag_blk_ - 1 << " , Transactions count: " << to_execute.size();
    }

    auto result = make_shared<FinalizationResult>(FinalizationResult{
        {
            new_blk.pbft_blk->getBeneficiary(),
            new_blk.pbft_blk->getTimestamp(),
            std::move(finalized_dag_blk_hashes),
            new_blk.pbft_blk->getBlockHash(),
        },
        blk_header,
        std::move(to_execute),
        std::move(receipts),
    });
    if (precommit_ext) {
      precommit_ext(*result, batch);
    }
    db_->commitWriteBatch(batch, db_opts_w_);
    state_api_.transition_state_commit();
    {
      unique_lock l(last_block_mu_);
      last_block_ = blk_header;
    }
    num_executed_dag_blk_ = num_executed_dag_blk;
    num_executed_trx_ = num_executed_trx;
    block_finalized_emitter_.emit(result);
    LOG(log_nf_) << " successful finalize block " << result->hash << " with number " << blk_header->number;
    // Creates snapshot if needed
    if (db_->createSnapshot(blk_header->number)) {
      state_api_.create_snapshot(blk_header->number);
    }
    return result;
  }

  shared_ptr<BlockHeader> append_block(DB::Batch& batch, addr_t const& author, uint64_t timestamp, uint64_t gas_limit,
                                       h256 const& state_root, Transactions const& transactions = {},
                                       TransactionReceipts const& receipts = {}) {
    auto blk_header_ptr = make_shared<BlockHeader>();
    auto& blk_header = *blk_header_ptr;
    auto last_block = block_header();
    blk_header.number = last_block ? last_block->number + 1 : 0;
    blk_header.parent_hash = last_block ? last_block->hash : h256();
    blk_header.author = author;
    blk_header.timestamp = timestamp;
    blk_header.state_root = state_root;
    blk_header.gas_used = receipts.empty() ? 0 : receipts.back().cumulative_gas_used;
    blk_header.gas_limit = gas_limit;
    BytesMap trxs_trie, receipts_trie;
    RLPStream rlp_strm;
    for (size_t i(0); i < transactions.size(); ++i) {
      auto const& trx = transactions[i];
      auto i_rlp = util::rlp_enc(rlp_strm, i);
      trxs_trie[i_rlp] = trx.rlp();
      auto const& receipt = receipts[i];
      receipts_trie[i_rlp] = util::rlp_enc(rlp_strm, receipt);
      db_->insert(batch, DB::Columns::final_chain_receipt_by_trx_hash, trx.getHash(), rlp_strm.out());
      auto bloom = receipt.bloom();
      blk_header.log_bloom |= bloom;
    }
    blk_header.transactions_root = hash256(trxs_trie);
    blk_header.receipts_root = hash256(receipts_trie);
    rlp_strm.clear(), blk_header.ethereum_rlp(rlp_strm);
    blk_header.ethereum_rlp_size = rlp_strm.out().size();
    blk_header.hash = sha3(rlp_strm.out());
    db_->insert(batch, DB::Columns::final_chain_blk_by_number, blk_header.number, util::rlp_enc(rlp_strm, blk_header));
    auto log_bloom_for_index = blk_header.log_bloom;
    log_bloom_for_index.shiftBloom<3>(sha3(blk_header.author.ref()));
    for (uint64_t level = 0, index = blk_header.number; level < c_bloomIndexLevels;
         ++level, index /= c_bloomIndexSize) {
      auto chunk_id = block_blooms_chunk_id(level, index / c_bloomIndexSize);
      auto chunk_to_alter = block_blooms(chunk_id);
      chunk_to_alter[index % c_bloomIndexSize] |= log_bloom_for_index;
      db_->insert(batch, DB::Columns::final_chain_log_blooms_index, chunk_id, util::rlp_enc(rlp_strm, chunk_to_alter));
    }
    TransactionLocation tl{blk_header.number};
    for (auto const& trx : transactions) {
      db_->insert(batch, DB::Columns::final_chain_transaction_location_by_hash, trx.getHash(),
                  util::rlp_enc(rlp_strm, tl));
      ++tl.index;
    }
    db_->insert(batch, DB::Columns::final_chain_transaction_hashes_by_blk_number, blk_header.number,
                TransactionHashesImpl::serialize_from_transactions(transactions));
    db_->insert(batch, DB::Columns::final_chain_transaction_count_by_blk_number, blk_header.number,
                transactions.size());
    db_->insert(batch, DB::Columns::final_chain_blk_hash_by_number, blk_header.number, blk_header.hash);
    db_->insert(batch, DB::Columns::final_chain_blk_number_by_hash, blk_header.hash, blk_header.number);
    db_->insert(batch, DB::Columns::final_chain_meta, LAST_NUMBER, blk_header.number);
    return blk_header_ptr;
  }

  shared_ptr<BlockHeader const> block_header(optional<EthBlockNumber> n = {}) const override {
    if (!n) {
      shared_lock l(last_block_mu_);
      return last_block_;
    }
    if (auto raw = db_->lookup(*n, DB::Columns::final_chain_blk_by_number); !raw.empty()) {
      auto ret = make_shared<BlockHeader>();
      ret->rlp(RLP(raw));
      return ret;
    }
    return {};
  }

  EthBlockNumber last_block_number() const override { return block_header()->number; }

  optional<EthBlockNumber> block_number(h256 const& h) const override {
    return db_->lookup_int<EthBlockNumber>(h, DB::Columns::final_chain_blk_number_by_hash);
  }

  optional<h256> block_hash(optional<EthBlockNumber> n = {}) const override {
    if (!n) {
      return block_header()->hash;
    }
    auto raw = db_->lookup(*n, DB::Columns::final_chain_blk_hash_by_number);
    if (raw.empty()) {
      return {};
    }
    return h256(raw, h256::FromBinary);
  };

  shared_ptr<TransactionHashes> transaction_hashes(optional<EthBlockNumber> n = {}) const override {
    return make_shared<TransactionHashesImpl>(
        db_->lookup(last_if_absent(n), DB::Columns::final_chain_transaction_hashes_by_blk_number));
  }

  optional<TransactionLocation> transaction_location(h256 const& trx_hash) const override {
    auto raw = db_->lookup(trx_hash, DB::Columns::final_chain_transaction_location_by_hash);
    if (raw.empty()) {
      return {};
    }
    TransactionLocation ret;
    ret.rlp(RLP(raw));
    return ret;
  }

  optional<TransactionReceipt> transaction_receipt(h256 const& trx_h) const override {
    auto raw = db_->lookup(trx_h, DB::Columns::final_chain_receipt_by_trx_hash);
    if (raw.empty()) {
      return {};
    }
    TransactionReceipt ret;
    ret.rlp(RLP(raw));
    return ret;
  }

  uint64_t transactionCount(optional<EthBlockNumber> n = {}) const override {
    return db_->lookup_int<uint64_t>(last_if_absent(n), DB::Columns::final_chain_transaction_count_by_blk_number)
        .value_or(0);
  }

  Transactions transactions(optional<EthBlockNumber> n = {}) const override {
    Transactions ret;
    auto hashes = transaction_hashes(n);
    ret.reserve(hashes->count());
    for (size_t i = 0; i < ret.capacity(); ++i) {
      auto trx = db_->getTransaction(hashes->get(i));
      assert(trx);
      ret.emplace_back(*trx);
    }
    return ret;
  }

  vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from, EthBlockNumber to) const override {
    vector<EthBlockNumber> ret;
    // start from the top-level
    auto u = int_pow(c_bloomIndexSize, c_bloomIndexLevels);
    // run through each of the top-level blocks
    for (EthBlockNumber index = from / u; index <= (to + u - 1) / u; ++index) {
      dev::operator+=(ret, withBlockBloom(b, from, to, c_bloomIndexLevels - 1, index));
    }
    return ret;
  }

  optional<state_api::Account> get_account(addr_t const& addr, optional<EthBlockNumber> blk_n = {}) const override {
    return state_api_.get_account(last_if_absent(blk_n), addr);
  }

  virtual u256 get_staking_balance(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const override {
    return state_api_.get_staking_balance(last_if_absent(blk_n), addr);
  }

  void update_state_config(const state_api::Config& new_config) const override {
    state_api_.update_state_config(new_config);
  }

  u256 get_account_storage(addr_t const& addr, u256 const& key, optional<EthBlockNumber> blk_n = {}) const override {
    return state_api_.get_account_storage(last_if_absent(blk_n), addr, key);
  }

  bytes get_code(addr_t const& addr, optional<EthBlockNumber> blk_n = {}) const override {
    return state_api_.get_code_by_address(last_if_absent(blk_n), addr);
  }

  state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<EthBlockNumber> blk_n = {},
                                  optional<state_api::ExecutionOptions> const& opts = {}) const override {
    auto const& blk_header = *block_header(last_if_absent(blk_n));
    return state_api_.dry_run_transaction(blk_header.number,
                                          {
                                              blk_header.author,
                                              blk_header.gas_limit,
                                              blk_header.timestamp,
                                              BlockHeader::difficulty(),
                                          },
                                          trx, opts);
  }

  uint64_t dpos_eligible_address_count(EthBlockNumber blk_num) const override {
    return state_api_.dpos_eligible_address_count(blk_num);
  }

  uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const override {
    return state_api_.dpos_eligible_total_vote_count(blk_num);
  }

  uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const override {
    return state_api_.dpos_eligible_vote_count(blk_num, addr);
  }

  bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const override {
    return state_api_.dpos_is_eligible(blk_num, addr);
  }

  state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                        std::optional<EthBlockNumber> blk_n = {}) const override {
    return state_api_.dpos_query(last_if_absent(blk_n), q);
  }

  bool is_nonce_valid(const addr_t& addr, const trx_nonce_t& nonce) const override {
    return !(replay_protection_service_ && replay_protection_service_->is_nonce_stale(addr, nonce));
  }

  EthBlockNumber last_if_absent(std::optional<EthBlockNumber> const& client_blk_n) const {
    return client_blk_n ? *client_blk_n : last_block_number();
  }

  static util::RangeView<state_api::EVMTransaction> to_state_api_transactions(Transactions const& trxs) {
    return util::make_range_view(trxs).map([](auto const& trx) {
      return state_api::EVMTransaction{
          trx.getSender(), trx.getGasPrice(), trx.getReceiver(), trx.getNonce(),
          trx.getValue(),  trx.getGas(),      trx.getData(),
      };
    });
  }

  BlocksBlooms block_blooms(h256 const& chunk_id) const {
    if (auto raw = db_->lookup(chunk_id, DB::Columns::final_chain_log_blooms_index); !raw.empty()) {
      return RLP(raw).toArray<LogBloom, c_bloomIndexSize>();
    }
    return {};
  }

  static h256 block_blooms_chunk_id(EthBlockNumber level, EthBlockNumber index) { return h256(index * 0xff + level); }

  vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from, EthBlockNumber to, EthBlockNumber level,
                                        EthBlockNumber index) const {
    vector<EthBlockNumber> ret;
    auto uCourse = int_pow(c_bloomIndexSize, level + 1);
    auto uFine = int_pow(c_bloomIndexSize, level);
    auto obegin = index == from / uCourse ? from / uFine % c_bloomIndexSize : 0;
    auto oend = index == to / uCourse ? (to / uFine) % c_bloomIndexSize + 1 : c_bloomIndexSize;
    auto bb = block_blooms(block_blooms_chunk_id(level, index));
    for (auto o = obegin; o < oend; ++o) {
      if (bb[o].contains(b)) {
        // This level has something like what we want.
        if (level > 0) {
          dev::operator+=(ret, withBlockBloom(b, from, to, level - 1, o + index * c_bloomIndexSize));
        } else {
          ret.push_back(o + index * c_bloomIndexSize);
        }
      }
    }
    return ret;
  }

  struct TransactionHashesImpl : TransactionHashes {
    string serialized_;
    size_t count_;

    explicit TransactionHashesImpl(string serialized)
        : serialized_(move(serialized)), count_(serialized_.size() / h256::size) {}

    static bytes serialize_from_transactions(Transactions const& transactions) {
      bytes serialized;
      serialized.reserve(transactions.size() * h256::size);
      for (auto const& trx : transactions) {
        for (auto b : trx.getHash()) {
          serialized.push_back(b);
        }
      }
      return serialized;
    }

    h256 get(size_t i) const override {
      return h256((uint8_t*)(serialized_.data() + i * h256::size), h256::ConstructFromPointer);
    }

    size_t count() const override { return count_; }
  };
};

shared_ptr<FinalChain> NewFinalChain(shared_ptr<DB> const& db, Config const& config, addr_t const& node_addr) {
  return make_shared<FinalChainImpl>(db, config, node_addr);
}

}  // namespace taraxa::final_chain
