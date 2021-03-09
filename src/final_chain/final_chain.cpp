#include "final_chain.hpp"

#include "TrieCommon.h"
#include "replay_protection_service.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "util/thread_pool.hpp"

namespace taraxa::final_chain {
using namespace std;
using namespace dev;

enum DBMetaKeys { LAST_PERIOD = 1 };

struct FinalChainImpl final : virtual FinalChain {
  shared_ptr<DB> db_;
  StateAPI state_api;
  unique_ptr<ReplayProtectionService> replay_protection_service_{new ReplayProtectionServiceDummy};

  mutable shared_mutex last_block_mu_;
  mutable shared_ptr<BlockHeader> last_block_;

  uint64_t transaction_count_hint_;
  util::ThreadPool executor_thread_{1};

  atomic<uint64_t> num_executed_dag_blk_ = 0;
  atomic<uint64_t> num_executed_trx_ = 0;

  LOG_OBJECTS_DEFINE;

  FinalChainImpl(shared_ptr<DB> const& db,  //
                 Config const& config,          //
                 FinalChain::Opts const& opts,  //
                 addr_t const& node_addr)
      : db_(db),
        state_api([this](auto n) { return block_hash(n).value_or(ZeroHash); },  //
                  config.state,
                  {
                      1500,
                      4,
                  },
                  {
                      (db->stateDbPath()).string(),
                  }),
        transaction_count_hint_(opts.state_api.ExpectedMaxTrxPerBlock) {
    LOG_OBJECTS_CREATE("EXECUTOR");
    auto state_db_descriptor = state_api.get_last_committed_state_descriptor();
    if (auto last_period = db_->lookup_int<BlockNumber>(DB::Columns::final_chain_meta, LAST_PERIOD); last_period) {
      last_block_ = make_shared<BlockHeader>();
      last_block_->rlp(RLP(db_->lookup(DB::Columns::final_chain_block_by_period, *last_period)));
      if (last_block_->number != state_db_descriptor.blk_num) {
        assert(state_db_descriptor.blk_num == last_block_->number - 1);
        auto res = state_api.transition_state(
            {
                last_block_->author,
                last_block_->gas_limit,
                last_block_->timestamp,
                BlockHeader::difficulty(),
            },
            to_state_api_transactions(transactions(last_block_->number)));
        assert(res.StateRoot == last_block_->state_root);
        state_api.transition_state_commit();
      }
    } else {
      assert(state_db_descriptor.blk_num == 0);
      auto batch = db->createWriteBatch();
      last_block_ = append_block(batch, config.genesis_block_fields.author, config.genesis_block_fields.timestamp, 0,
                                 state_db_descriptor.state_root);
      batch.commit();
    }
    num_executed_dag_blk_ = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
    num_executed_trx_ = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  }

  void finalize(shared_ptr<PbftBlock> pbft_blk) override {
    assert(pbft_blk);
    executor_thread_.post([this, pbft_blk] { finalize_(pbft_blk); });
  }

  void finalize_(shared_ptr<PbftBlock> pbft_blk_ptr) {
    auto const& pbft_block = *pbft_blk_ptr;
    auto pbft_period = pbft_block.getPeriod();
    auto const& pbft_block_hash = pbft_block.getBlockHash();
    auto const& anchor_hash = pbft_block.getPivotDagBlockHash();
    auto finalized_dag_blk_hashes = db_->getFinalizedDagBlockHashesByAnchor(anchor_hash);
    auto batch = db_->createWriteBatch();

    Transactions to_execute;
    {
      // This artificial scope will make sure we clean up the big chunk of memory allocated for this batch-processing
      // stuff as soon as possible
      DB::MultiGetQuery db_query(db_, 2 * transaction_count_hint_ + finalized_dag_blk_hashes.size());
      auto dag_blks_raw = db_query.append(DB::Columns::dag_blocks, finalized_dag_blk_hashes, false).execute();
      unordered_set<h256> unique_trxs;
      unique_trxs.reserve(transaction_count_hint_);
      for (auto const& dag_blk_raw : dag_blks_raw) {
        for (auto const& trx_h : DagBlock::extract_transactions_from_rlp(RLP(dag_blk_raw))) {
          batch.remove(DB::Columns::pending_transactions, trx_h);
          if (!unique_trxs.insert(trx_h).second) {
            continue;
          }
          db_query.append(DB::Columns::executed_transactions, trx_h);
          db_query.append(DB::Columns::transactions, trx_h);
        }
      }
      auto trx_db_results = db_query.execute(false);
      to_execute.reserve(unique_trxs.size());
      for (uint i = 0; i < unique_trxs.size(); ++i) {
        if (auto has_been_executed = !trx_db_results[0 + i * 2].empty(); has_been_executed) {
          continue;
        }
        // Non-executed trxs
        auto const& trx = to_execute.emplace_back(vector_ref<string::value_type>(trx_db_results[1 + i * 2]).toBytes(),
                                                  false, h256(db_query.get_key(1 + i * 2)));
        if (replay_protection_service_->is_nonce_stale(trx.getSender(), trx.getNonce())) {
          to_execute.pop_back();
        }
      }
    }
    transaction_count_hint_ = max(transaction_count_hint_, to_execute.size());

    constexpr auto gas_limit = std::numeric_limits<uint64_t>::max();
    auto const& [exec_results, state_root] = state_api.transition_state(
        {
            pbft_block.getBeneficiary(),
            gas_limit,
            pbft_block.getTimestamp(),
            BlockHeader::difficulty(),
        },
        to_state_api_transactions(to_execute));
    TransactionReceipts receipts;
    receipts.reserve(exec_results.size());
    gas_t cumulative_gas_used = 0;
    for (auto const& r : exec_results) {
      LogEntries logs;
      logs.reserve(r.Logs.size());
      for (auto const& l : r.Logs) {
        logs.push_back({l.Address, l.Topics, l.Data});
      }
      receipts.push_back(TransactionReceipt{
          r.CodeErr.empty() && r.ConsensusErr.empty(),
          r.GasUsed,
          cumulative_gas_used += r.GasUsed,
          move(logs),
          r.NewContractAddr ? optional(r.NewContractAddr) : nullopt,
      });
    }

    auto blk_header = append_block(batch, pbft_block.getBeneficiary(), pbft_block.getTimestamp(), gas_limit, state_root,
                                   to_execute, receipts);

    // Update replay protection service, like nonce watermark. Nonce watermark has been disabled
    replay_protection_service_->update(batch, pbft_period, util::make_range_view(to_execute).map([](auto const& trx) {
      return ReplayProtectionService::TransactionInfo{trx.getSender(), trx.getNonce()};
    }));

    // Update number of executed DAG blocks and transactions
    auto num_executed_dag_blk = num_executed_dag_blk_ + finalized_dag_blk_hashes.size();
    auto num_executed_trx = num_executed_trx_ + to_execute.size();
    if (!finalized_dag_blk_hashes.empty()) {
      batch.addStatusField(StatusDbField::ExecutedBlkCount, num_executed_dag_blk);
      batch.addStatusField(StatusDbField::ExecutedTrxCount, num_executed_trx);
      LOG(log_nf_) << "Executed dag blocks #" << num_executed_dag_blk_ - finalized_dag_blk_hashes.size() << "-"
                   << num_executed_dag_blk_ - 1 << " , Transactions count: " << to_execute.size();
    }

    batch.commit();
    state_api.transition_state_commit();

    LOG(log_nf_) << "DB write batch committed at period " << pbft_period << " PBFT block hash " << pbft_block_hash;

    // Creates snapshot if needed
    if (db_->createSnapshot(pbft_period)) {
      state_api.create_snapshot(pbft_period);
    }

    // Only NOW we are fine to modify in-memory states as they have been backed by the db
    num_executed_dag_blk_ = num_executed_dag_blk;
    num_executed_trx_ = num_executed_trx;
    {
      unique_lock l(last_block_mu_);
      last_block_ = blk_header;
    }

    block_executed_.emit({pbft_blk_ptr, move(finalized_dag_blk_hashes), blk_header, move(to_execute), move(receipts)});

    LOG(log_nf_) << " successful execute pbft block " << pbft_block_hash << " in period " << pbft_period;
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
    BlockLogBlooms blk_log_blooms;
    blk_log_blooms.reserve(transactions.size());
    RLPStream rlp_strm;
    for (size_t i(0); i < transactions.size(); ++i) {
      auto const& trx = transactions[i];
      rlp_strm.clear(), util::rlp(rlp_strm, i);
      auto i_rlp = rlp_strm.out();
      trxs_trie[i_rlp] = *trx.rlp();
      auto const& receipt = receipts[i];
      rlp_strm.clear(), util::rlp(rlp_strm, receipt);
      receipts_trie[i_rlp] = rlp_strm.out();
      batch.put(DB::Columns::final_chain_receipt_by_trx_hash, trx.getHash(), rlp_strm.out());
      auto bloom = receipt.bloom();
      blk_header.log_bloom |= bloom;
      blk_log_blooms.push_back(bloom);
    }
    blk_header.transactions_root = hash256(trxs_trie);
    blk_header.receipts_root = hash256(receipts_trie);
    rlp_strm.clear(), blk_header.ethereum_rlp(rlp_strm);
    blk_header.ethereum_rlp_size = rlp_strm.out().size();
    blk_header.hash = sha3(rlp_strm.out());
    rlp_strm.clear(), blk_header.rlp(rlp_strm);
    batch.put(DB::Columns::final_chain_block_by_period, blk_header.number, rlp_strm.out());
    rlp_strm.clear(), util::rlp(rlp_strm, blk_log_blooms);
    batch.put(DB::Columns::final_chain_log_blooms, blk_header.number, rlp_strm.out());
    auto log_bloom_for_index = blk_header.log_bloom;
    log_bloom_for_index.shiftBloom<3>(sha3(blk_header.author.ref()));
    for (uint64_t level = 0, index = blk_header.number; level < c_bloomIndexLevels;
         ++level, index /= c_bloomIndexSize) {
      auto chunk_id = block_blooms_chunk_id(level, index / c_bloomIndexSize);
      auto prev_value = block_blooms(chunk_id);
      prev_value[index % c_bloomIndexSize] |= log_bloom_for_index;
      rlp_strm.clear(), util::rlp(rlp_strm, prev_value);
      batch.put(DB::Columns::final_chain_log_blooms_index, chunk_id, rlp_strm.out());
    }
    TransactionLocation tl{blk_header.number};
    for (auto const& trx : transactions) {
      rlp_strm.clear(), util::rlp(rlp_strm, tl);
      batch.put(DB::Columns::executed_transactions, trx.getHash(), rlp_strm.out());
      ++tl.index;
    }
    batch.put(DB::Columns::executed_transactions_by_period, blk_header.number,
              TransactionHashesImpl::serialize_from_transactions(transactions));
    batch.put(DB::Columns::executed_transactions_count_by_period, blk_header.number, transactions.size());
    batch.put(DB::Columns::final_chain_block_hash_by_period, blk_header.number, blk_header.hash);
    batch.put(DB::Columns::period_by_final_chain_block_hash, blk_header.hash, blk_header.number);
    batch.put(DB::Columns::final_chain_meta, LAST_PERIOD, blk_header.number);
    return blk_header_ptr;
  }

  shared_ptr<BlockHeader> block_header(optional<BlockNumber> n = {}) const override {
    if (!n) {
      shared_lock l(last_block_mu_);
      return last_block_;
    }
    auto ret = make_shared<BlockHeader>();
    ret->rlp(RLP(db_->lookup(DB::Columns::final_chain_block_by_period, *n)));
    return ret;
  }

  BlockNumber last_block_number() const override { return block_header()->number; }

  optional<BlockNumber> block_number(h256 const& h) const override {
    return db_->lookup_int<BlockNumber>(DB::Columns::period_by_final_chain_block_hash, h);
  }

  optional<h256> block_hash(optional<BlockNumber> n = {}) const override {
    if (!n) {
      return block_header()->hash;
    }
    auto raw = db_->lookup(DB::Columns::final_chain_block_hash_by_period, *n);
    if (raw.empty()) {
      return {};
    }
    return h256(raw, h256::FromBinary);
  };

  shared_ptr<TransactionHashes> transaction_hashes(optional<BlockNumber> n = {}) const override {
    return make_shared<TransactionHashesImpl>(db_->lookup(DB::Columns::final_chain_block_by_period, last_if_absent(n)));
  }

  optional<TransactionLocation> transaction_location(h256 const& trx_hash) const override {
    auto raw = db_->lookup(DB::Columns::executed_transactions, trx_hash);
    if (raw.empty()) {
      return {};
    }
    TransactionLocation ret;
    ret.rlp(RLP(raw));
    return ret;
  }

  optional<TransactionReceipt> transaction_receipt(h256 const& trx_h) const override {
    auto raw = db_->lookup(DB::Columns::final_chain_receipt_by_trx_hash, trx_h);
    if (raw.empty()) {
      return {};
    }
    TransactionReceipt ret;
    ret.rlp(RLP(raw));
    return ret;
  }

  uint64_t transactionCount(optional<BlockNumber> n = {}) const override {
    return db_->lookup_int<uint64_t>(DB::Columns::executed_transactions_count_by_period, last_if_absent(n)).value_or(0);
  }

  Transactions transactions(optional<BlockNumber> n = {}) const override {
    Transactions ret;
    auto hashes = transaction_hashes(n);
    ret.reserve(hashes->count());
    hashes->for_each([&](auto const& hash) { ret.emplace_back(*db_->getTransaction(hash)); });
    return ret;
  }

  vector<BlockNumber> withBlockBloom(LogBloom const& b, BlockNumber from, BlockNumber to) const override {
    vector<BlockNumber> ret;
    // start from the top-level
    auto u = int_pow(c_bloomIndexSize, c_bloomIndexLevels);
    // run through each of the top-level blocks
    for (BlockNumber index = from / u; index <= (to + u - 1) / u; ++index) {
      dev::operator+=(ret, withBlockBloom(b, from, to, c_bloomIndexLevels - 1, index));
    }
    return ret;
  }

  optional<state_api::Account> get_account(addr_t const& addr, optional<BlockNumber> blk_n = {}) const override {
    return state_api.get_account(last_if_absent(blk_n), addr);
  }

  u256 get_account_storage(addr_t const& addr, u256 const& key, optional<BlockNumber> blk_n = {}) const override {
    return state_api.get_account_storage(last_if_absent(blk_n), addr, key);
  }

  bytes get_code(addr_t const& addr, optional<BlockNumber> blk_n = {}) const override {
    return state_api.get_code_by_address(last_if_absent(blk_n), addr);
  }

  state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<BlockNumber> blk_n = {},
                                  optional<state_api::ExecutionOptions> const& opts = {}) const override {
    auto const& blk_header = *block_header(last_if_absent(blk_n));
    return state_api.dry_run_transaction(blk_header.number,
                                         {
                                             blk_header.author,
                                             blk_header.gas_limit,
                                             blk_header.timestamp,
                                             BlockHeader::difficulty(),
                                         },
                                         trx, opts);
  }

  uint64_t dpos_eligible_count(BlockNumber blk_num) const override { return state_api.dpos_eligible_count(blk_num); }

  bool dpos_is_eligible(BlockNumber blk_num, addr_t const& addr) const override {
    return state_api.dpos_is_eligible(blk_num, addr);
  }

  state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                        optional<BlockNumber> blk_n = {}) const override {
    return state_api.dpos_query(last_if_absent(blk_n), q);
  }

  BlockNumber last_if_absent(optional<BlockNumber> const& client_blk_n) const {
    return client_blk_n ? *client_blk_n : last_block_number();
  }

  static util::RangeView<state_api::EVMTransaction> to_state_api_transactions(Transactions const& trxs) {
    return make_range_view(trxs).map([](auto const& trx) {
      return state_api::EVMTransaction{
          trx.getSender(), trx.getGasPrice(), trx.getReceiver(), trx.getNonce(),
          trx.getValue(),  trx.getGas(),      trx.getData(),
      };
    });
  }

  BlocksBlooms block_blooms(h256 const& chunk_id) const {
    if (auto raw = db_->lookup(DB::Columns::final_chain_log_blooms_index, chunk_id); !raw.empty()) {
      return RLP(raw).toArray<LogBloom, c_bloomIndexSize>();
    }
    return {};
  }

  static h256 block_blooms_chunk_id(BlockNumber level, BlockNumber index) { return h256(index * 0xff + level); }

  vector<BlockNumber> withBlockBloom(LogBloom const& b, BlockNumber from, BlockNumber to, BlockNumber level,
                                     BlockNumber index) const {
    vector<BlockNumber> ret;
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

    TransactionHashesImpl(string serialized) : serialized_(move(serialized)) {}

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

    size_t count() const override { return serialized_.size() / h256::size; }

    void for_each(function<void(h256 const&)> const& cb) const override {
      h256 hash;
      size_t i = 0;
      for (auto b : serialized_) {
        hash[i] = b;
        if (++i; i % h256::size == 0) {
          cb(hash);
        }
      }
    }
  };
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DB> const& db,  //
                                     FinalChain::Config const& config,  //
                                     FinalChain::Opts const& opts, addr_t const& node_addr) {
  return util::u_ptr(new FinalChainImpl(db, config, opts, node_addr));
}

Json::Value enc_json(FinalChain::Config const& obj) {
  Json::Value json(Json::objectValue);
  json["state"] = enc_json(obj.state);
  json["genesis_block_fields"] = enc_json(obj.genesis_block_fields);
  return json;
}

void dec_json(Json::Value const& json, FinalChain::Config& obj) {
  dec_json(json["state"], obj.state);
  dec_json(json["genesis_block_fields"], obj.genesis_block_fields);
}

Json::Value enc_json(FinalChain::Config::GenesisBlockFields const& obj) {
  Json::Value json(Json::objectValue);
  json["timestamp"] = dev::toJS(obj.timestamp);
  json["author"] = dev::toJS(obj.author);
  return json;
}

void dec_json(Json::Value const& json, FinalChain::Config::GenesisBlockFields& obj) {
  obj.timestamp = dev::jsToInt(json["timestamp"].asString());
  obj.author = addr_t(json["author"].asString());
}

}  // namespace taraxa::final_chain
