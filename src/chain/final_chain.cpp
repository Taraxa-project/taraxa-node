#include "final_chain.hpp"

#include <libdevcore/CommonJS.h>

namespace taraxa::final_chain {

auto map_transactions(Transactions const& trxs) {
  return make_range_view(trxs).map([](auto const& trx) {
    return state_api::EVMTransaction{
        trx.from(), trx.gasPrice(), trx.isCreation() ? std::nullopt : optional(trx.to()), trx.nonce(), trx.value(),
        trx.gas(),  trx.data(),
    };
  });
}

struct FinalChainImpl : virtual FinalChain, virtual ChainDBImpl {
  shared_ptr<aleth::Database> blk_db;
  shared_ptr<aleth::Database> ext_db;
  StateAPI state_api;
  TransactionReceipts receipts_buf;

  FinalChainImpl(shared_ptr<DbStorage> db,
                 Config const& config,     //
                 decltype(blk_db) blk_db,  //
                 decltype(ext_db) ext_db,  //
                 FinalChain::Opts const& opts)
      : ChainDBImpl(blk_db, ext_db),
        blk_db(move(blk_db)),
        ext_db(move(ext_db)),
        state_api([this](auto n) { return ChainDBImpl::hashFromNumber(n); },  //
                  config.state,
                  {
                      1500,
                      4,
                  },
                  {
                      (db->stateDbStoragePath()).string(),
                  }) {
    receipts_buf.reserve(opts.state_api.ExpectedMaxTrxPerBlock);
    auto last_blk = ChainDBImpl::get_last_block();
    auto state_desc = state_api.get_last_committed_state_descriptor();
    if (!last_blk) {
      assert(state_desc.blk_num == 0);
      auto batch = db->createWriteBatch();
      auto exit_stack = append_block_prepare(batch);
      append_block(config.genesis_block_fields.author, config.genesis_block_fields.timestamp, 0, state_desc.state_root,
                   {}, {});
      db->commitWriteBatch(batch);
      refresh_last_block();
      return;
    }
    auto blk_n = last_blk->number();
    if (blk_n == state_desc.blk_num) {
      return;
    }
    if (state_desc.blk_num != blk_n - 1) {
      cerr << "state db last executed block number (" << state_desc.blk_num << ") is expected to be " << blk_n - 1
           << endl;
      exit(1);
    }
    auto res = state_api.transition_state(
        {
            last_blk->author(),
            last_blk->gasLimit(),
            last_blk->timestamp(),
            last_blk->difficulty(),
        },
        map_transactions(ChainDBImpl::transactions(blk_n)),  //
        {});
    assert(res.StateRoot == last_blk->stateRoot());
    state_api.transition_state_commit();
  }

  std::pair<val_t, bool> getBalance(addr_t const& addr) const override {
    if (auto acc = get_account(addr)) {
      return {acc->Balance, true};
    }
    return {0, false};
  }

  util::ExitStack append_block_prepare(DbStorage::BatchPtr const& batch) {
    blk_db->setBatch(batch);
    ext_db->setBatch(batch);
    return [this] {
      blk_db->setBatch(nullptr);
      ext_db->setBatch(nullptr);
    };
  }

  shared_ptr<BlockHeader> get_last_block() const override { return ChainDBImpl::get_last_block(); }

  EthBlockNumber normalize_client_blk_n(optional<EthBlockNumber> const& client_blk_n) const {
    return client_blk_n ? *client_blk_n : last_block_number();
  }

  AdvanceResult advance(DbStorage::BatchPtr batch, Address const& author, uint64_t timestamp,
                        Transactions const& transactions) override {
    constexpr auto gas_limit = ((uint64_t)1 << 53) - 1;
    auto const& state_transition_result = state_api.transition_state(
        {
            author,
            gas_limit,
            timestamp,
            0,
        },
        map_transactions(transactions),  //
        {});
    receipts_buf.clear();
    receipts_buf.reserve(state_transition_result.ExecutionResults.size());
    gas_t cumulative_gas_used = 0;
    for (auto const& r : state_transition_result.ExecutionResults) {
      LogEntries logs;
      logs.reserve(r.Logs.size());
      for (auto const& l : r.Logs) {
        logs.emplace_back(l.Address, l.Topics, l.Data);
      }
      receipts_buf.emplace_back(r.CodeErr.empty() && r.ConsensusErr.empty(), cumulative_gas_used += r.GasUsed,
                                move(logs), r.NewContractAddr);
    }
    auto exit_stack = append_block_prepare(batch);
    return {
        append_block(author, timestamp, gas_limit, state_transition_result.StateRoot, transactions, receipts_buf),
        receipts_buf,
        state_transition_result,
    };
  }

  void advance_confirm() override {
    state_api.transition_state_commit();
    refresh_last_block();
  }

  void create_snapshot(uint64_t const& period) override { state_api.create_snapshot(period); }

  optional<state_api::Account> get_account(addr_t const& addr,
                                           optional<EthBlockNumber> blk_n = nullopt) const override {
    return state_api.get_account(normalize_client_blk_n(blk_n), addr);
  }

  u256 get_account_storage(addr_t const& addr, u256 const& key,
                           optional<EthBlockNumber> blk_n = nullopt) const override {
    return state_api.get_account_storage(normalize_client_blk_n(blk_n), addr, key);
  }

  bytes get_code(addr_t const& addr, optional<EthBlockNumber> blk_n = nullopt) const override {
    return state_api.get_code_by_address(normalize_client_blk_n(blk_n), addr);
  }

  state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<EthBlockNumber> blk_n = nullopt,
                                  optional<state_api::ExecutionOptions> const& opts = nullopt) const override {
    auto blk_header = blockHeader(normalize_client_blk_n(blk_n));
    return state_api.dry_run_transaction(blk_header.number(),
                                         {
                                             blk_header.author(),
                                             blk_header.gasLimit(),
                                             blk_header.timestamp(),
                                             blk_header.difficulty(),
                                         },
                                         trx, opts);
  }

  uint64_t dpos_eligible_count(EthBlockNumber blk_num) const override { return state_api.dpos_eligible_count(blk_num); }

  uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const override {
    return state_api.dpos_eligible_total_vote_count(blk_num);
  }

  virtual uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const override {
    return state_api.dpos_eligible_vote_count(blk_num, addr);
  }

  bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const override {
    return state_api.dpos_is_eligible(blk_num, addr);
  }

  state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                        optional<EthBlockNumber> blk_n = nullopt) const override {
    return state_api.dpos_query(normalize_client_blk_n(blk_n), q);
  }
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DbStorage> db, FinalChain::Config const& config,
                                     FinalChain::Opts const& opts) {
  return u_ptr(new FinalChainImpl(db, config, aleth::NewDatabase(db, DbStorage::Columns::aleth_chain),
                                  aleth::NewDatabase(db, DbStorage::Columns::aleth_chain_extras), opts));
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
