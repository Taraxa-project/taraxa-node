#include "final_chain.hpp"

#include "ChainDBImpl.h"
#include "replay_protection_service.hpp"
#include "transaction_manager/transaction_manager.hpp"
#include "util/signal.hpp"

namespace taraxa::final_chain {
using namespace std;
using namespace dev;

struct FinalChainImpl final : virtual FinalChain, virtual ChainDBImpl {
  shared_ptr<DbStorage> db_;
  shared_ptr<PbftChain> pbft_chain_;
  StateAPI state_api;
  unique_ptr<ReplayProtectionService> replay_protection_service_{new ReplayProtectionServiceDummy};

  util::Signal<shared_ptr<PbftBlock>> to_execute_;
  unique_ptr<thread> exec_worker_;

  Transactions transactions_;
  atomic<uint64_t> num_executed_dag_blk_ = 0;
  atomic<uint64_t> num_executed_trx_ = 0;

  LOG_OBJECTS_DEFINE;

  FinalChainImpl(shared_ptr<DbStorage> const& db,  //
                 shared_ptr<PbftChain> pbft_chain,
                 Config const& config,          //
                 FinalChain::Opts const& opts,  //
                 addr_t const& node_addr)
      : db_(db),
        pbft_chain_(move(pbft_chain)),
        state_api([this](auto n) { return ChainDBImpl::hashFromNumber(n); },  //
                  config.state,
                  {
                      1500,
                      4,
                  },
                  {
                      (db->stateDbStoragePath()).string(),
                  }) {
    LOG_OBJECTS_CREATE("EXECUTOR");
    auto state_db_descriptor = state_api.get_last_committed_state_descriptor();
    if (auto last_blk_raw = db_->lookup(DbStorage::Columns::default_column, LAST_BLOCK_KEY); last_blk_raw.empty()) {
      assert(state_db_descriptor.blk_num == 0);
      auto batch = db->createWriteBatch();
      last_block_ = append_block(batch, config.genesis_block_fields.author, config.genesis_block_fields.timestamp, 0,
                                 state_db_descriptor.state_root, {}, {});
      batch.commit();
    } else if ((last_block_ = make_shared<BlockHeader>())->rlp(RLP(last_blk_raw)),
               last_block_->number != state_db_descriptor.blk_num) {
      assert(state_db_descriptor.blk_num == last_block_->number - 1);
      auto res = state_api.transition_state(
          {last_block_->author, last_block_->gas_limit, last_block_->timestamp, BlockHeader::difficulty()},
          map_transactions(transactions(last_block_->number)),  //
          {});
      assert(res.StateRoot == last_block_->state_root);
      state_api.transition_state_commit();
    }
    transactions_.reserve(opts.state_api.ExpectedMaxTrxPerBlock);
    num_executed_dag_blk_ = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
    num_executed_trx_ = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
    if (auto last_period = pbft_chain_->getPbftChainSize(); last_block_->number < last_period) {
      execute(pbft_chain_->getPbftBlock(last_period));
    }
    exec_worker_ = std::make_unique<std::thread>([this]() {
      LOG(log_nf_) << "Executor run...";
      for (;;) {
        auto pbft_block = to_execute_.await();
        if (!pbft_block) {
          break;
        }
        for (auto blk_n = last_block_number() + 1; blk_n < pbft_block->getPeriod(); ++blk_n) {
          execute_(pbft_chain_->getPbftBlock(blk_n));
        }
        execute_(pbft_block);
      }
    });
  }

  ~FinalChainImpl() override {
    to_execute_.emit(nullptr);
    exec_worker_->join();
    LOG(log_nf_) << "Executor stopped";
  }

  void execute(shared_ptr<PbftBlock> pbft_blk) override {
    assert(pbft_blk);
    to_execute_.emit(move(pbft_blk));
  }

  void execute_(shared_ptr<PbftBlock> const& pbft_blk_ptr) {
    auto const& pbft_block = *pbft_blk_ptr;
    auto pbft_period = pbft_block.getPeriod();
    auto const& pbft_block_hash = pbft_block.getBlockHash();
    auto const& anchor_hash = pbft_block.getPivotDagBlockHash();
    auto finalized_dag_blk_hashes = db_->getFinalizedDagBlockHashesByAnchor(anchor_hash);
    auto batch = db_->createWriteBatch();
    {
      // This artificial scope will make sure we clean up the big chunk of memory allocated for this batch-processing
      // stuff as soon as possible
      DbStorage::MultiGetQuery db_query(db_, transactions_.capacity() + 100);
      auto dag_blks_raw = db_query.append(DbStorage::Columns::dag_blocks, finalized_dag_blk_hashes, false).execute();
      unordered_set<h256> unique_trxs;
      unique_trxs.reserve(transactions_.capacity());
      for (auto const& dag_blk_raw : dag_blks_raw) {
        for (auto const& trx_h : DagBlock::extract_transactions_from_rlp(RLP(dag_blk_raw))) {
          batch.remove(DbStorage::Columns::pending_transactions, trx_h);
          if (!unique_trxs.insert(trx_h).second) {
            continue;
          }
          db_query.append(DbStorage::Columns::executed_transactions, trx_h);
          db_query.append(DbStorage::Columns::transactions, trx_h);
        }
      }
      auto trx_db_results = db_query.execute(false);
      for (uint i = 0; i < unique_trxs.size(); ++i) {
        if (auto has_been_executed = !trx_db_results[0 + i * 2].empty(); has_been_executed) {
          continue;
        }
        // Non-executed trxs
        auto const& trx =
            transactions_.emplace_back(vector_ref<string::value_type>(trx_db_results[1 + i * 2]).toBytes(), false,
                                       h256(db_query.get_key(1 + i * 2)));
        if (replay_protection_service_->is_nonce_stale(trx.getSender(), trx.getNonce())) {
          transactions_.pop_back();
        }
      }
    }

    constexpr auto gas_limit = std::numeric_limits<uint64_t>::max();
    auto const& [exec_results, state_root] = state_api.transition_state(
        {pbft_block.getBeneficiary(), gas_limit, pbft_block.getTimestamp(), BlockHeader::difficulty()},
        map_transactions(transactions_));
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
          r.NewContractAddr,
      });
    }

    auto blk_header = append_block(batch, pbft_block.getBeneficiary(), pbft_block.getTimestamp(), gas_limit, state_root,
                                   transactions_, receipts);

    // Update replay protection service, like nonce watermark. Nonce watermark has been disabled
    replay_protection_service_->update(
        batch, pbft_period, util::make_range_view(transactions_).map([](auto const& trx) {
          return ReplayProtectionService::TransactionInfo{trx.getSender(), trx.getNonce()};
        }));

    // Update number of executed DAG blocks and transactions
    auto num_executed_dag_blk = num_executed_dag_blk_ + finalized_dag_blk_hashes.size();
    auto num_executed_trx = num_executed_trx_ + transactions_.size();
    if (!finalized_dag_blk_hashes.empty()) {
      batch.addStatusField(StatusDbField::ExecutedBlkCount, num_executed_dag_blk);
      batch.addStatusField(StatusDbField::ExecutedTrxCount, num_executed_trx);
      LOG(log_nf_) << "Executed dag blocks #" << num_executed_dag_blk_ - finalized_dag_blk_hashes.size() << "-"
                   << num_executed_dag_blk_ - 1 << " , Transactions count: " << transactions_.size();
    }

    batch.commit();
    state_api.transition_state_commit();

    LOG(log_nf_) << "DB write batch committed at period " << pbft_period << " PBFT block hash " << pbft_block_hash;

    // Creates snapshot if needed
    if (db_->createSnapshot(pbft_period)) {
      create_state_db_snapshot(pbft_period);
    }

    // Only NOW we are fine to modify in-memory states as they have been backed by the db
    num_executed_dag_blk_ = num_executed_dag_blk;
    num_executed_trx_ = num_executed_trx;
    {
      unique_lock l(last_block_mu_);
      last_block_ = blk_header;
    }

    block_executed_.emit({pbft_blk_ptr, move(finalized_dag_blk_hashes), blk_header, move(receipts)});

    transactions_.clear();

    LOG(log_nf_) << " successful execute pbft block " << pbft_block_hash << " in period " << pbft_period;
  }

  shared_ptr<BlockHeader> append_block(DbStorage::Batch& batch, addr_t const& author, uint64_t timestamp,
                                       uint64_t gas_limit, h256 const& state_root, Transactions const& transactions,
                                       TransactionReceipts const& receipts) {
    auto blk_header_ptr = make_shared<BlockHeader>();
    auto& blk_header = *blk_header_ptr;
    auto last_block = get_last_block();
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
      batch.put(DbStorage::Columns::final_chain_receipts, trx.getHash(), rlp_strm.out());
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
    batch.put(DbStorage::Columns::final_chain_blocks, blk_header.hash, rlp_strm.out());
    rlp_strm.clear(), util::rlp(rlp_strm, blk_log_blooms);
    batch.put(DbStorage::Columns::final_chain_log_blooms, blk_header.hash, rlp_strm.out());
    auto log_bloom_for_index = blk_header.log_bloom;
    log_bloom_for_index.shiftBloom<3>(sha3(blk_header.author.ref()));
    for (uint64_t level = 0, index = blk_header.number; level < c_bloomIndexLevels;
         ++level, index /= c_bloomIndexSize) {
      auto chunk_id = chunkId(level, index / c_bloomIndexSize);
      auto prev_value = blocksBlooms(chunk_id);
      prev_value[index % c_bloomIndexSize] |= log_bloom_for_index;
      rlp_strm.clear(), util::rlp(rlp_strm, prev_value);
      batch.put(DbStorage::Columns::final_chain_log_blooms_index, chunk_id, rlp_strm.out());
    }
    TransactionLocation tl{blk_header.hash};
    for (auto const& trx : transactions) {
      rlp_strm.clear(), util::rlp(rlp_strm, tl);
      batch.put(DbStorage::Columns::executed_transactions, trx.getHash(), rlp_strm.out());
      ++tl.index;
    }
    batch.put(DbStorage::Columns::final_chain_block_number_to_hash, blk_header.number, blk_header.hash);
    batch.put(DbStorage::Columns::default_column, LAST_BLOCK_KEY, blk_header.hash);
    return blk_header_ptr;
  }

  shared_ptr<BlockHeader> get_last_block() const override {
    shared_lock l(last_block_mu_);
    return last_block_;
  }

  optional<state_api::Account> get_account(addr_t const& addr, optional<BlockNumber> blk_n = nullopt) const override {
    return state_api.get_account(normalize_client_blk_n(blk_n), addr);
  }

  std::pair<val_t, bool> getBalance(addr_t const& addr) const override {
    if (auto acc = get_account(addr)) {
      return {acc->Balance, true};
    }
    return {0, false};
  }

  u256 get_account_storage(addr_t const& addr, u256 const& key, optional<BlockNumber> blk_n = nullopt) const override {
    return state_api.get_account_storage(normalize_client_blk_n(blk_n), addr, key);
  }

  bytes get_code(addr_t const& addr, optional<BlockNumber> blk_n = nullopt) const override {
    return state_api.get_code_by_address(normalize_client_blk_n(blk_n), addr);
  }

  state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<BlockNumber> blk_n = nullopt,
                                  optional<state_api::ExecutionOptions> const& opts = nullopt) const override {
    auto blk_header = blockHeader(normalize_client_blk_n(blk_n));
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
                                        optional<BlockNumber> blk_n = nullopt) const override {
    return state_api.dpos_query(normalize_client_blk_n(blk_n), q);
  }

  BlockNumber normalize_client_blk_n(optional<BlockNumber> const& client_blk_n) const {
    return client_blk_n ? *client_blk_n : last_block_number();
  }

  util::RangeView<state_api::EVMTransaction> map_transactions(Transactions const& trxs) {
    return make_range_view(trxs).map([](auto const& trx) {
      return state_api::EVMTransaction{
          trx.getSender(), trx.getGasPrice(), trx.getReceiver(), trx.getNonce(),
          trx.getValue(),  trx.getGas(),      trx.getData(),
      };
    });
  }

  void create_state_db_snapshot(uint64_t const& period) override { state_api.create_snapshot(period); }
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DbStorage> db,  //
                                     shared_ptr<PbftChain> pbft_chain,
                                     FinalChain::Config const& config,  //
                                     FinalChain::Opts const& opts, addr_t const& node_addr) {
  return util::u_ptr(new FinalChainImpl(move(db), move(pbft_chain), config, opts, node_addr));
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
