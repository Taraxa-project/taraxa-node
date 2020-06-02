#include "final_chain.hpp"

namespace taraxa::final_chain {

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
        state_api([&, this, last_block = get_last_block()] {
          return StateAPI(
              *db,  //
              [this](auto n) { return hashFromNumber(n); },
              config.state.chain_config,  //
              last_block ? last_block->number() : 0,
              last_block ? last_block->stateRoot() : h256());  //
        }()) {
    receipts_buf.reserve(opts.state_api.ExpectedMaxNumTrxPerBlock);
    if (get_last_block()) {
      return;
    }
    auto batch = db->createWriteBatch();
    auto genesis_state_root = state_api.StateTransition_ApplyAccounts(
        *batch, config.state.genesis_accounts);
    auto exit_stack = append_block_prepare(batch);
    append_block(config.genesis_block_fields.author,
                 config.genesis_block_fields.timestamp, 0, genesis_state_root,
                 {}, {});
    db->commitWriteBatch(batch);
    advance_confirm();
  }

  util::ExitStack append_block_prepare(DbStorage::BatchPtr const& batch) {
    blk_db->setBatch(batch);
    ext_db->setBatch(batch);
    return [this] {
      blk_db->setBatch(nullptr);
      ext_db->setBatch(nullptr);
    };
  }

  shared_ptr<BlockHeader> get_last_block() const override {
    return ChainDBImpl::get_last_block();
  }

  AdvanceResult advance(DbStorage::BatchPtr batch, Address const& author,
                        uint64_t timestamp,
                        Transactions const& transactions) override {
    constexpr auto gas_limit = std::numeric_limits<uint64_t>::max();
    auto const& state_transition_result = state_api.StateTransition_ApplyBlock(
        *batch,
        {
            author,
            gas_limit,
            timestamp,
            0,
        },
        make_range_view(transactions).map([](auto const& trx) {
          assert(trx.nonce() <= numeric_limits<uint64_t>::max());
          assert(trx.gas() <= numeric_limits<uint64_t>::max());
          return state_api::EVMTransaction{
              trx.from(),
              trx.gasPrice(),
              trx.isCreation() ? std::nullopt : optional(trx.to()),
              trx.nonce(),
              trx.value(),
              trx.gas(),
              trx.data(),
          };
        }),
        {},  //
        {});
    receipts_buf.clear();
    receipts_buf.reserve(state_transition_result.ExecutionResults.size());
    for (auto const& r : state_transition_result.ExecutionResults) {
      LogEntries logs;
      logs.reserve(r.Logs.size());
      for (auto const& l : r.Logs) {
        logs.emplace_back(l.Address, l.Topics, l.Data);
      }
      receipts_buf.emplace_back(r.CodeErr.empty() && r.ConsensusErr.empty(),
                                r.GasUsed, move(logs), r.NewContractAddr);
    }
    auto exit_stack = append_block_prepare(batch);
    return {
        append_block(author, timestamp, gas_limit,
                     state_transition_result.StateRoot, transactions,
                     receipts_buf),
        receipts_buf,
        state_transition_result,
    };
  }

  void advance_confirm() override {
    state_api.NotifyStateTransitionCommitted();
    refresh_last_block();
  }

  optional<state_api::Account> get_account(
      addr_t const& addr,
      optional<BlockNumber> blk_n = nullopt) const override {
    return state_api.Historical_GetAccount(blk_n ? *blk_n : last_block_number(),
                                           addr);
  }

  u256 get_account_storage(
      addr_t const& addr, u256 const& key,
      optional<BlockNumber> blk_n = nullopt) const override {
    return state_api.Historical_GetAccountStorage(
        blk_n ? *blk_n : last_block_number(), addr, key);
  }

  bytes get_code(addr_t const& addr,
                 optional<BlockNumber> blk_n = nullopt) const override {
    return state_api.Historical_GetCodeByAddress(
        blk_n ? *blk_n : last_block_number(), addr);
  }

  state_api::ExecutionResult call(state_api::EVMTransaction const& trx,
                                  optional<BlockNumber> blk_n = nullopt,
                                  optional<state_api::ExecutionOptions> const&
                                      opts = nullopt) const override {
    auto blk_header = blk_n ? blockHeader(*blk_n) : *get_last_block();
    return state_api.DryRunner_Apply(blk_header.number(),
                                     {
                                         blk_header.author(),
                                         blk_header.gasLimit(),
                                         blk_header.timestamp(),
                                         blk_header.difficulty(),
                                     },
                                     trx, opts);
  }
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DbStorage> db,
                                     FinalChain::Config const& config,
                                     FinalChain::Opts const& opts) {
  return u_ptr(new FinalChainImpl(
      db, config, aleth::NewDatabase(db, DbStorage::Columns::aleth_chain),
      aleth::NewDatabase(db, DbStorage::Columns::aleth_chain_extras), opts));
}

}  // namespace taraxa::final_chain
