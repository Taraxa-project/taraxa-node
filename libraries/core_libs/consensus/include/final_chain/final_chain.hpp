#pragma once

#include <future>

#include "common/event.hpp"
#include "common/range_view.hpp"
#include "common/types.hpp"
#include "config/chain_config.hpp"
#include "final_chain/data.hpp"
#include "final_chain/state_api.hpp"
#include "storage/storage.hpp"

namespace taraxa::final_chain {

/** @addtogroup FinalChain
 * @{
 */

enum class DBMetaKeys { LAST_NUMBER = 1 };

/**
 * @brief main responsibility is blocks execution in EVM, getting data from EVM state
 *
 * Class have methods which you can call to get info from EVM and related state.
 * Main thing here is finalize() method, that is called to execute some pbft block.
 * It implements few events and implements methods related to Ethereum API
 */
class FinalChain {
 public:
  static constexpr auto GAS_LIMIT = ((uint64_t)1 << 53) - 1;
  using finalize_precommit_ext = std::function<void(FinalizationResult const&, DB::Batch&)>;

 protected:
  util::EventEmitter<std::shared_ptr<FinalizationResult>> const block_finalized_emitter_{};
  util::EventEmitter<uint64_t> const block_applying_emitter_{};

 public:
  decltype(block_finalized_emitter_)::Subscriber const& block_finalized_ = block_finalized_emitter_;
  decltype(block_applying_emitter_)::Subscriber const& block_applying_ = block_applying_emitter_;

  FinalChain() = default;
  virtual ~FinalChain() = default;
  FinalChain(const FinalChain&) = delete;
  FinalChain(FinalChain&&) = delete;
  FinalChain& operator=(const FinalChain&) = delete;
  FinalChain& operator=(FinalChain&&) = delete;

  virtual void stop() = 0;

  /**
   * @brief Method which finalizes a block and executes it in EVM
   *
   * @param period_data Full period data with transactions
   * @param finalized_dag_blk_hashes
   * @param precommit_ext
   * @return finalization result
   */
  virtual std::future<std::shared_ptr<FinalizationResult const>> finalize(PeriodData&& period_data,
                                                                          std::vector<h256>&& finalized_dag_blk_hashes,
                                                                          finalize_precommit_ext = {}) = 0;

  /**
   * @brief Method to get block header by block number
   *
   * @param n block number of header to get. If not specified then it returns latest
   * @return BlockHeader
   */
  virtual std::shared_ptr<BlockHeader const> block_header(std::optional<EthBlockNumber> n = {}) const = 0;

  /**
   * @brief Method to get last block number(chain size)
   * @return EthBlockNumber
   */
  virtual EthBlockNumber last_block_number() const = 0;

  /**
   * @brief Method to get block number by hash
   *
   * @param h block hash
   * @return EthBlockNumber
   */
  virtual std::optional<EthBlockNumber> block_number(h256 const& h) const = 0;

  /**
   * @brief Method to get block hash by block number
   *
   * @param n EthBlockNumber
   * @return BlockHash h256
   */
  virtual std::optional<h256> block_hash(std::optional<EthBlockNumber> n = {}) const = 0;
  struct TransactionHashes {
    TransactionHashes() = default;
    virtual ~TransactionHashes() = default;
    TransactionHashes(const TransactionHashes&) = default;
    TransactionHashes(TransactionHashes&&) = default;
    TransactionHashes& operator=(const TransactionHashes&) = default;
    TransactionHashes& operator=(TransactionHashes&&) = default;

    virtual size_t count() const = 0;
    virtual h256 get(size_t i) const = 0;
  };

  /**
   * @brief Needed if we are changing params with hardfork and it affects Go part of code. For example DPOS contract
   * @param new_config state_api::Config
   */
  virtual void update_state_config(const state_api::Config& new_config) = 0;

  /**
   * @brief Method to get all transaction hashes from the block
   * @param n EthBlockNumber
   * @return TransactionHashes list if transaction hashes
   */
  virtual std::shared_ptr<TransactionHashes> transaction_hashes(std::optional<EthBlockNumber> n = {}) const = 0;

  /**
   * @brief Method to get all transactions from the block
   * @param n EthBlockNumber
   * @return SharedTransactions vector of shared_ptrs to Transaction
   */
  virtual SharedTransactions transactions(std::optional<EthBlockNumber> n = {}) const = 0;

  /**
   * @brief Method to get transaction location by hash
   * @param trx_hash hash of transaction to get location for
   * @return std::optional<TransactionLocation> transaction location or nullopt
   */
  virtual std::optional<TransactionLocation> transaction_location(h256 const& trx_hash) const = 0;

  /**
   * @brief Method to get transaction receipt by hash
   * @param _transactionHash hash of transaction to get receipt for
   * @return std::optional<TransactionReceipt> transaction receipt or nullopt
   */
  virtual std::optional<TransactionReceipt> transaction_receipt(h256 const& _transactionHash) const = 0;

  /**
   * @brief Method to get transactions count in block
   * @param n block number
   * @return count of transactions in block
   */
  virtual uint64_t transactionCount(std::optional<EthBlockNumber> n = {}) const = 0;

  /**
   * @brief Method used to search for contract logs with bloom filter
   * @param b LogBloom
   * @param from EthBlockNumber block to start search
   * @param to EthBlockNumber block to end search
   * @return block that matches specified bloom filter
   */
  virtual std::vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from,
                                                     EthBlockNumber to) const = 0;

  /**
   * @brief Method to get account information
   * @see state_api::Account
   * @param addr account address
   * @param blk_n number of block we are getting state from
   * @return std::optional<state_api::Account> account object or nullopt if account wasn't found
   */
  virtual std::optional<state_api::Account> get_account(addr_t const& addr,
                                                        std::optional<EthBlockNumber> blk_n = {}) const = 0;
  /**
   * @brief Method to get staking balance from DPOS precompiled contract
   * @param addr account address
   * @param blk_n number of block we are getting state from
   * @return staking balance
   */
  virtual u256 get_staking_balance(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const = 0;

  /**
   * @brief Returns the value from a storage position at a given address.
   * @param addr account address
   * @param key position in the storage
   * @param blk_n number of block we are getting state from
   * @return the value at this storage position
   */
  virtual u256 get_account_storage(addr_t const& addr, u256 const& key,
                                   std::optional<EthBlockNumber> blk_n = {}) const = 0;
  /**
   * @brief Returns code at a given address.
   * @param addr account address
   * @param blk_n number of block we are getting state from
   * @return code at a given address.
   */
  virtual bytes get_code(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const = 0;

  /**
   * @brief Executes a new message call immediately without creating a transaction on the block chain. That means that
   * state would be reverted and not saved anywhere
   * @param trx state_api::EVMTransaction
   * @param blk_n EthBlockNumber number of block we are getting state from
   * @param opts state_api::ExecutionOptions
   * @return state_api::ExecutionResult
   */
  virtual state_api::ExecutionResult call(state_api::EVMTransaction const& trx,
                                          std::optional<EthBlockNumber> blk_n = {},
                                          std::optional<state_api::ExecutionOptions> const& opts = {}) const = 0;

  /**
   * @brief total count of eligible votes are in DPOS precompiled contract
   * @param blk_num EthBlockNumber number of block we are getting state from
   * @return total count of eligible votes
   */
  virtual uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const = 0;

  /**
   * @brief total count of eligible votes account has in DPOS precompiled contract
   * @param blk_num EthBlockNumber number of block we are getting state from
   * @param addr account address
   * @return address eligible votes count
   */
  virtual uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const = 0;

  /**
   * @brief method to check if address have enough votes to participate in consensus
   * @param blk_num EthBlockNumber number of block we are getting state from
   * @param addr account address
   * @return is address eligible
   */
  virtual bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const = 0;
  // TODO move out of here:

  std::pair<val_t, bool> getBalance(addr_t const& addr) const {
    if (auto acc = get_account(addr)) {
      return {acc->balance, true};
    }
    return {0, false};
  }
};

std::shared_ptr<FinalChain> NewFinalChain(const std::shared_ptr<DB>& db, const taraxa::ChainConfig& config,
                                          const addr_t& node_addr = {});
/** @} */

}  // namespace taraxa::final_chain

namespace taraxa {
using final_chain::FinalChain;
using final_chain::NewFinalChain;
}  // namespace taraxa
