#pragma once

#include <rocksdb/db.h>
#include <taraxa-evm/taraxa-evm.h>

#include <functional>

#include "state_api_data.hpp"
#include "storage/db_storage.hpp"
#include "util/range_view.hpp"

namespace taraxa::state_api {

class StateAPI {
  function<h256(EthBlockNumber)> get_blk_hash_;
  taraxa_evm_GetBlockHash get_blk_hash_c_;
  taraxa_evm_state_API_ptr this_c_;
  RLPStream rlp_enc_transition_state_;
  StateTransitionResult result_buf_transition_state_;
  string db_path_;

 public:
  StateAPI(function<h256(EthBlockNumber)> get_blk_hash, Config const& chain_config, Opts const& opts,
           OptsDB const& opts_db);
  ~StateAPI();

  Proof prove(EthBlockNumber blk_num, root_t const& state_root, addr_t const& addr, vector<h256> const& keys) const;
  optional<Account> get_account(EthBlockNumber blk_num, addr_t const& addr) const;
  u256 get_account_storage(EthBlockNumber blk_num, addr_t const& addr, u256 const& key) const;
  bytes get_code_by_address(EthBlockNumber blk_num, addr_t const& addr) const;
  ExecutionResult dry_run_transaction(EthBlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
                                      optional<ExecutionOptions> const& opts = nullopt) const;
  StateDescriptor get_last_committed_state_descriptor() const;
  StateTransitionResult const& transition_state(EVMBlock const& block,
                                                RangeView<EVMTransaction> const& transactions,  //
                                                RangeView<UncleBlock> const& uncles = {});
  void transition_state_commit();
  void create_snapshot(uint64_t period);
  // DPOS
  uint64_t dpos_eligible_count(EthBlockNumber blk_num) const;
  uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const;
  uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const;

  bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const;
  DPOSQueryResult dpos_query(EthBlockNumber blk_num, DPOSQuery const& q) const;
  static addr_t const& dpos_contract_addr();
  struct DPOSTransactionPrototype {
    uint64_t minimal_gas = 0;  // TODO estimate gas
    byte value = 0;
    bytes input;
    addr_t const& to = dpos_contract_addr();

    DPOSTransactionPrototype(DPOSTransfers const& transfers);
  };
};

}  // namespace taraxa::state_api

namespace taraxa {
using state_api::StateAPI;
}
