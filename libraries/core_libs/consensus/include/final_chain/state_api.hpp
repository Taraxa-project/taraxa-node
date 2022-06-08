#pragma once

#include <taraxa-evm/taraxa-evm.h>

#include <functional>

#include "common/range_view.hpp"
#include "final_chain/rewards_stats.hpp"
#include "final_chain/state_api_data.hpp"
#include "storage/storage.hpp"

namespace taraxa::state_api {

/** @addtogroup FinalChain
 * @{
 */

class StateAPI {
  std::function<h256(EthBlockNumber)> get_blk_hash_;
  taraxa_evm_GetBlockHash get_blk_hash_c_;
  taraxa_evm_state_API_ptr this_c_;
  dev::RLPStream rlp_enc_transition_state_;
  StateTransitionResult result_buf_transition_state_;
  string db_path_;

 public:
  StateAPI(std::function<h256(EthBlockNumber)> get_blk_hash, Config const& chain_config, Opts const& opts,
           OptsDB const& opts_db);
  ~StateAPI();
  StateAPI(const StateAPI&) = default;
  StateAPI(StateAPI&&) = default;
  StateAPI& operator=(const StateAPI&) = default;
  StateAPI& operator=(StateAPI&&) = default;

  void update_state_config(const Config& new_config);

  Proof prove(EthBlockNumber blk_num, root_t const& state_root, addr_t const& addr,
              std::vector<h256> const& keys) const;
  std::optional<Account> get_account(EthBlockNumber blk_num, addr_t const& addr) const;
  u256 get_account_storage(EthBlockNumber blk_num, addr_t const& addr, u256 const& key) const;
  bytes get_code_by_address(EthBlockNumber blk_num, addr_t const& addr) const;
  ExecutionResult dry_run_transaction(EthBlockNumber blk_num, EVMBlock const& blk, EVMTransaction const& trx,
                                      std::optional<ExecutionOptions> const& opts = std::nullopt) const;
  StateDescriptor get_last_committed_state_descriptor() const;
  StateTransitionResult const& transition_state(const EVMBlock& block,
                                                const util::RangeView<EVMTransaction>& transactions,
                                                const util::RangeView<addr_t>& transactions_validators = {},
                                                const util::RangeView<UncleBlock>& uncles = {},
                                                const RewardsStats& rewards_stats = {});
  void transition_state_commit();
  void create_snapshot(uint64_t period);
  // DPOS
  uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const;
  uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const;

  bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const;
  u256 get_staking_balance(EthBlockNumber blk_num, const addr_t& addr) const;
  static addr_t const& dpos_contract_addr();
};
/** @} */

}  // namespace taraxa::state_api

namespace taraxa {
using state_api::StateAPI;
}
