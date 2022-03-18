#pragma once

#include <future>

#include "common/event.hpp"
#include "common/range_view.hpp"
#include "common/types.hpp"
#include "config/final_chain_config.hpp"
#include "final_chain/data.hpp"
#include "final_chain/state_api.hpp"
#include "storage/storage.hpp"

namespace taraxa::final_chain {

class FinalChain {
 public:
  static constexpr auto GAS_LIMIT = ((uint64_t)1 << 53) - 1;

 protected:
  util::EventEmitter<std::shared_ptr<FinalizationResult>> const block_finalized_emitter_{};
  util::EventEmitter<uint64_t> const block_applying_emitter_{};

 public:
  decltype(block_finalized_emitter_)::Subscriber const& block_finalized_ = block_finalized_emitter_;
  decltype(block_applying_emitter_)::Subscriber const& block_applying = block_applying_emitter_;

  virtual ~FinalChain() = default;
  virtual void stop() = 0;

  using finalize_precommit_ext = std::function<void(FinalizationResult const&, DB::Batch&)>;
  virtual std::future<std::shared_ptr<FinalizationResult const>> finalize(SyncBlock&& sync_block,
                                                                          std::vector<h256>&& finalized_dag_blk_hashes,
                                                                          finalize_precommit_ext = {}) = 0;

  virtual std::shared_ptr<BlockHeader const> block_header(std::optional<EthBlockNumber> n = {}) const = 0;
  virtual EthBlockNumber last_block_number() const = 0;
  virtual std::optional<EthBlockNumber> block_number(h256 const& h) const = 0;
  virtual std::optional<h256> block_hash(std::optional<EthBlockNumber> n = {}) const = 0;
  struct TransactionHashes {
    virtual ~TransactionHashes() {}

    virtual size_t count() const = 0;
    virtual h256 get(size_t i) const = 0;
  };

  virtual void update_state_config(const state_api::Config& new_config) const = 0;
  virtual std::shared_ptr<TransactionHashes> transaction_hashes(std::optional<EthBlockNumber> n = {}) const = 0;
  virtual Transactions transactions(std::optional<EthBlockNumber> n = {}) const = 0;
  virtual std::optional<TransactionLocation> transaction_location(h256 const& trx_hash) const = 0;
  virtual std::optional<TransactionReceipt> transaction_receipt(h256 const& _transactionHash) const = 0;
  virtual uint64_t transactionCount(std::optional<EthBlockNumber> n = {}) const = 0;
  virtual std::vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from,
                                                     EthBlockNumber to) const = 0;

  virtual std::optional<state_api::Account> get_account(addr_t const& addr,
                                                        std::optional<EthBlockNumber> blk_n = {}) const = 0;
  virtual u256 get_staking_balance(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const = 0;
  virtual u256 get_account_storage(addr_t const& addr, u256 const& key,
                                   std::optional<EthBlockNumber> blk_n = {}) const = 0;
  virtual bytes get_code(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const = 0;
  virtual state_api::ExecutionResult call(state_api::EVMTransaction const& trx,
                                          std::optional<EthBlockNumber> blk_n = {},
                                          std::optional<state_api::ExecutionOptions> const& opts = {}) const = 0;

  virtual uint64_t dpos_eligible_address_count(EthBlockNumber blk_num) const = 0;
  virtual uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const = 0;
  virtual uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const = 0;

  virtual bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const = 0;
  virtual state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                                std::optional<EthBlockNumber> blk_n = {}) const = 0;

  virtual bool is_nonce_valid(const addr_t& addr, const trx_nonce_t& nonce) const = 0;

  // TODO move out of here:

  std::pair<val_t, bool> getBalance(addr_t const& addr) const {
    if (auto acc = get_account(addr)) {
      return {acc->balance, true};
    }
    return {0, false};
  }
};

std::shared_ptr<FinalChain> NewFinalChain(std::shared_ptr<DB> const& db, Config const& config,
                                          addr_t const& node_addr = {});

}  // namespace taraxa::final_chain

namespace taraxa {
using final_chain::FinalChain;
using final_chain::NewFinalChain;
}  // namespace taraxa
