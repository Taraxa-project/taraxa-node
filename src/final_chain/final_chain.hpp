#pragma once

#include <future>

#include "common/types.hpp"
#include "consensus/pbft_chain.hpp"
#include "data.hpp"
#include "state_api.hpp"
#include "storage/db_storage.hpp"
#include "util/event.hpp"
#include "util/exit_stack.hpp"
#include "util/range_view.hpp"

namespace taraxa::final_chain {
using namespace ::std;
using namespace ::dev;
using namespace ::taraxa::final_chain;
using namespace ::taraxa::util;

struct FinalChain {
  static constexpr auto GAS_LIMIT = ((uint64_t)1 << 53) - 1;

  struct Config {
    state_api::ChainConfig state;
    struct GenesisBlockFields {
      addr_t author;
      uint64_t timestamp = 0;
    } genesis_block_fields;
  };

  struct Opts {
    state_api::Opts state_api;
  };

  struct NewBlock {
    addr_t author;
    uint64_t timestamp;
    vector<h256> dag_blk_hashes;
    h256 hash;
  };

  struct FinalizationResult : NewBlock {
    shared_ptr<BlockHeader const> final_chain_blk;
    Transactions trxs;
    TransactionReceipts trx_receipts;
  };

 protected:
  util::EventEmitter<shared_ptr<FinalizationResult>> const block_finalized_{};

 public:
  decltype(block_finalized_)::Subscriber const& block_finalized = block_finalized_;

  virtual ~FinalChain() = default;

  using finalize_precommit_ext = std::function<void(FinalizationResult const&, DB::Batch&)>;
  virtual future<shared_ptr<FinalizationResult const>> finalize(NewBlock new_blk, finalize_precommit_ext = {}) = 0;

  virtual shared_ptr<BlockHeader const> block_header(optional<EthBlockNumber> n = {}) const = 0;
  virtual EthBlockNumber last_block_number() const = 0;
  virtual optional<EthBlockNumber> block_number(h256 const& h) const = 0;
  virtual optional<h256> block_hash(optional<EthBlockNumber> n = {}) const = 0;
  struct TransactionHashes {
    virtual ~TransactionHashes() {}

    virtual size_t count() const = 0;
    virtual h256 get(size_t i) const = 0;
    virtual void for_each(function<void(h256 const&)> const& cb) const = 0;
  };
  virtual shared_ptr<TransactionHashes> transaction_hashes(optional<EthBlockNumber> n = {}) const = 0;
  virtual Transactions transactions(optional<EthBlockNumber> n = {}) const = 0;
  virtual optional<TransactionLocation> transaction_location(h256 const& trx_hash) const = 0;
  virtual optional<TransactionReceipt> transaction_receipt(h256 const& _transactionHash) const = 0;
  virtual uint64_t transactionCount(optional<EthBlockNumber> n = {}) const = 0;
  virtual vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from, EthBlockNumber to) const = 0;

  virtual optional<state_api::Account> get_account(addr_t const& addr, optional<EthBlockNumber> blk_n = {}) const = 0;
  virtual u256 get_account_storage(addr_t const& addr, u256 const& key, optional<EthBlockNumber> blk_n = {}) const = 0;
  virtual bytes get_code(addr_t const& addr, optional<EthBlockNumber> blk_n = {}) const = 0;
  virtual state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<EthBlockNumber> blk_n = {},
                                          optional<state_api::ExecutionOptions> const& opts = {}) const = 0;

  virtual uint64_t dpos_eligible_count(EthBlockNumber blk_num) const = 0;
  virtual uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const = 0;
  virtual uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const = 0;

  virtual bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const = 0;
  virtual state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                                optional<EthBlockNumber> blk_n = {}) const = 0;

  // TODO move out of here:

  pair<val_t, bool> getBalance(addr_t const& addr) const {
    if (auto acc = get_account(addr)) {
      return {acc->Balance, true};
    }
    return {0, false};
  }
};

unique_ptr<FinalChain> NewFinalChain(shared_ptr<DB> const& db,           //
                                     FinalChain::Config const& config,   //
                                     FinalChain::Opts const& opts = {},  //
                                     addr_t const& node_addr = {});

Json::Value enc_json(FinalChain::Config const& obj);
void dec_json(Json::Value const& json, FinalChain::Config& obj);
Json::Value enc_json(FinalChain::Config::GenesisBlockFields const& obj);
void dec_json(Json::Value const& json, FinalChain::Config::GenesisBlockFields& obj);

}  // namespace taraxa::final_chain

namespace taraxa {
using final_chain::FinalChain;
using final_chain::NewFinalChain;
}  // namespace taraxa
