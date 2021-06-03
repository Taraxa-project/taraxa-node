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

  struct BlockFinalized {
    shared_ptr<PbftBlock> pbft_blk;
    vector<blk_hash_t> finalized_dag_blk_hashes;
    shared_ptr<BlockHeader const> final_chain_blk;
    Transactions trxs;
    TransactionReceipts trx_receipts;
  };

 protected:
  util::EventEmitter<shared_ptr<BlockFinalized>> const block_finalized_;

 public:
  decltype(block_finalized_)::Subscriber const& block_finalized = block_finalized_;

  virtual ~FinalChain() {}

  virtual future<shared_ptr<BlockFinalized>> finalize(shared_ptr<PbftBlock> pbft_blk) = 0;

  virtual shared_ptr<BlockHeader> block_header(optional<BlockNumber> n = {}) const = 0;
  virtual BlockNumber last_block_number() const = 0;
  virtual optional<BlockNumber> block_number(h256 const& h) const = 0;
  virtual optional<h256> block_hash(optional<BlockNumber> n = {}) const = 0;
  struct TransactionHashes {
    virtual ~TransactionHashes() {}

    virtual size_t count() const = 0;
    virtual h256 get(size_t i) const = 0;
    virtual void for_each(function<void(h256 const&)> const& cb) const = 0;
  };
  virtual shared_ptr<TransactionHashes> transaction_hashes(optional<BlockNumber> n = {}) const = 0;
  virtual Transactions transactions(optional<BlockNumber> n = {}) const = 0;
  virtual optional<TransactionLocation> transaction_location(h256 const& trx_hash) const = 0;
  virtual optional<TransactionReceipt> transaction_receipt(h256 const& _transactionHash) const = 0;
  virtual uint64_t transactionCount(optional<BlockNumber> n = {}) const = 0;
  virtual vector<BlockNumber> withBlockBloom(LogBloom const& b, BlockNumber from, BlockNumber to) const = 0;

  virtual optional<state_api::Account> get_account(addr_t const& addr, optional<BlockNumber> blk_n = {}) const = 0;
  virtual u256 get_account_storage(addr_t const& addr, u256 const& key, optional<BlockNumber> blk_n = {}) const = 0;
  virtual bytes get_code(addr_t const& addr, optional<BlockNumber> blk_n = {}) const = 0;
  virtual state_api::ExecutionResult call(state_api::EVMTransaction const& trx, optional<BlockNumber> blk_n = {},
                                          optional<state_api::ExecutionOptions> const& opts = {}) const = 0;

  virtual uint64_t dpos_eligible_count(BlockNumber blk_num) const = 0;
  virtual bool dpos_is_eligible(BlockNumber blk_num, addr_t const& addr) const = 0;
  virtual state_api::DPOSQueryResult dpos_query(state_api::DPOSQuery const& q,
                                                optional<BlockNumber> blk_n = {}) const = 0;

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
