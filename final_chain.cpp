#include "final_chain.hpp"

namespace taraxa::final_chain {

FinalChain::FinalChain(shared_ptr<DbStorage> db, Config const& config)
    : bc_db(new aleth::Database(db, DbStorage::Columns::aleth_chain)),
      bc_extras_db(
          new aleth::Database(db, DbStorage::Columns::aleth_chain_extras)),
      bc(new BlockDB(bc_db, bc_extras_db)),
      replay_protection_service(new ReplayProtectionService(
          config.replay_protection_service_range, db)) {
  auto last_block = get_last_block();
  state_api = StateAPI::New(
      *db,  //
      [this](auto n) { return bc->hashFromNumber(n); },
      config.state.chain_config,  //
      last_block ? last_block->number() : 0,
      last_block ? last_block->stateRoot() : h256());  //
  if (last_block) {
    return;
  }
  auto batch = db->createWriteBatch();
  auto genesis_state_root = state_api->StateTransition_ApplyAccounts(
      *batch, config.state.genesis_accounts);
  auto exit_stack = append_block_prepare(batch);
  bc->append_block(config.genesis_block_fields.author,
                   config.genesis_block_fields.timestamp, 0, genesis_state_root,
                   {}, {});
  db->commitWriteBatch(batch);
  advance_confirm();
}

FinalChain::AdvanceResult FinalChain::advance(
    DbStorage::BatchPtr batch, Address const& author, uint64_t timestamp,
    Transactions const& transactions) {
  auto blk_num = bc->last_block_number() + 1;
  constexpr auto gas_limit = std::numeric_limits<uint64_t>::max();
  auto state_transition_result = state_api->StateTransition_ApplyBlock(
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
        return StateAPI::EVMTransaction{
            trx.from(),
            trx.gasPrice(),
            trx.isCreation() ? std::nullopt : optional(trx.to()),
            trx.nonce().template convert_to<uint64_t>(),
            trx.value(),
            trx.gas().template convert_to<uint64_t>(),
            trx.data(),
        };
      }),
      {},  //
      {});
  replay_protection_service->register_executed_transactions(
      batch, blk_num, make_range_view(transactions).map([](auto const& trx) {
        return ReplayProtectionService::TransactionInfo{
            trx.from(),
            trx.nonce().template convert_to<uint64_t>(),
        };
      }));
  TransactionReceipts receipts;
  receipts.reserve(state_transition_result.ExecutionResults.size());
  for (auto const& r : state_transition_result.ExecutionResults) {
    LogEntries logs;
    logs.reserve(r.Logs.size());
    for (auto const& l : r.Logs) {
      logs.emplace_back(move(l.Address), move(l.Topics), move(l.Data));
    }
    receipts.emplace_back(r.CodeErr.empty() && r.ConsensusErr.empty(),
                          r.GasUsed, move(logs));
  }
  auto exit_stack = append_block_prepare(batch);
  return {
      bc->append_block(author, timestamp, gas_limit,
                       state_transition_result.StateRoot, transactions,
                       receipts),
      move(receipts),
      move(state_transition_result.BalanceChanges),
  };
}

void FinalChain::advance_confirm() {
  state_api->NotifyStateTransitionCommitted();
  bc->refresh_last_block();
}

util::ExitStack FinalChain::append_block_prepare(DbStorage::BatchPtr batch) {
  bc_db->setBatch(batch);
  bc_extras_db->setBatch(batch);
  return [this] {
    bc_db->setBatch(nullptr);
    bc_extras_db->setBatch(nullptr);
  };
}

}  // namespace taraxa::final_chain
