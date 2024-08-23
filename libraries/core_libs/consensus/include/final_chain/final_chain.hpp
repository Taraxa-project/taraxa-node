#pragma once

#include <future>

#include "common/event.hpp"
#include "common/types.hpp"
#include "config/config.hpp"
#include "final_chain/cache.hpp"
#include "final_chain/data.hpp"
#include "final_chain/state_api.hpp"
#include "final_chain/state_api_data.hpp"
#include "rewards/rewards_stats.hpp"
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
 protected:
  util::EventEmitter<std::shared_ptr<FinalizationResult>> const block_finalized_emitter_{};
  util::EventEmitter<uint64_t> const block_applying_emitter_{};

 public:
  decltype(block_finalized_emitter_)::Subscriber const& block_finalized_ = block_finalized_emitter_;
  decltype(block_applying_emitter_)::Subscriber const& block_applying_ = block_applying_emitter_;

  FinalChain() = default;
  ~FinalChain() = default;
  FinalChain(const std::shared_ptr<DB>& db, const taraxa::FullNodeConfig& config, const addr_t& node_addr);
  FinalChain(const FinalChain&) = delete;
  FinalChain(FinalChain&&) = delete;
  FinalChain& operator=(const FinalChain&) = delete;
  FinalChain& operator=(FinalChain&&) = delete;

  void stop();

  EthBlockNumber delegationDelay() const;

  /**
   * @brief Method which finalizes a block and executes it in EVM
   *
   * @param period_data Full period data with transactions
   * @param finalized_dag_blk_hashes
   * @param precommit_ext
   * @return finalization result
   */
  std::future<std::shared_ptr<const FinalizationResult>> finalize(PeriodData&& period_data,
                                                                  std::vector<h256>&& finalized_dag_blk_hashes,
                                                                  std::shared_ptr<DagBlock>&& anchor = nullptr);

  /**
   * @brief Method to get block header by block number
   *
   * @param n block number of header to get. If not specified then it returns latest
   * @return BlockHeader
   */
  std::shared_ptr<const BlockHeader> blockHeader(std::optional<EthBlockNumber> n = {}) const;

  /**
   * @brief Method to get last block number(chain size)
   * @return EthBlockNumber
   */
  EthBlockNumber lastBlockNumber() const;

  /**
   * @brief Method to get block number by hash
   *
   * @param h block hash
   * @return EthBlockNumber
   */
  std::optional<EthBlockNumber> blockNumber(h256 const& h) const;

  /**
   * @brief Method to get block hash by block number
   *
   * @param n EthBlockNumber
   * @return BlockHash h256
   */
  std::optional<h256> blockHash(std::optional<EthBlockNumber> n = {}) const;

  /**
   * @brief Needed if we are changing params with hardfork and it affects Go part of code. For example DPOS contract
   * @param new_config state_api::Config
   */
  void updateStateConfig(const state_api::Config& new_config);

  /**
   * @brief Method to get all transaction hashes from the block
   * @param n EthBlockNumber
   * @return TransactionHashes list if transaction hashes
   */
  std::shared_ptr<const TransactionHashes> transactionHashes(std::optional<EthBlockNumber> n = {}) const;

  /**
   * @brief Method to get all transactions from the block
   * @param n EthBlockNumber
   * @return SharedTransactions vector of shared_ptrs to Transaction
   */
  const SharedTransactions transactions(std::optional<EthBlockNumber> n = {}) const;

  /**
   * @brief Method to get transaction location by hash
   * @param trx_hash hash of transaction to get location for
   * @return std::optional<TransactionLocation> transaction location or nullopt
   */
  std::optional<TransactionLocation> transactionLocation(h256 const& trx_hash) const;

  /**
   * @brief Method to get transaction receipt by hash
   * @param _transactionHash hash of transaction to get receipt for
   * @return std::optional<TransactionReceipt> transaction receipt or nullopt
   */
  std::optional<TransactionReceipt> transactionReceipt(h256 const& _transactionHash) const;

  /**
   * @brief Method to get transactions count in block
   * @param n block number
   * @return count of transactions in block
   */
  uint64_t transactionCount(std::optional<EthBlockNumber> n = {}) const;

  /**
   * @brief Method used to search for contract logs with bloom filter
   * @param b LogBloom
   * @param from EthBlockNumber block to start search
   * @param to EthBlockNumber block to end search
   * @return block that matches specified bloom filter
   */
  std::vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from, EthBlockNumber to) const;

  /**
   * @brief Method to get account information
   * @see state_api::Account
   * @param addr account address
   * @param blk_n number of block we are getting state from
   * @return std::optional<state_api::Account> account object or nullopt if account wasn't found
   */
  std::optional<state_api::Account> getAccount(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const;

  /**
   * @brief Returns the value from a storage position at a given address.
   * @param addr account address
   * @param key position in the storage
   * @param blk_n number of block we are getting state from
   * @return the value at this storage position
   */
  h256 getAccountStorage(addr_t const& addr, u256 const& key, std::optional<EthBlockNumber> blk_n = {}) const;
  /**
   * @brief Returns code at a given address.
   * @param addr account address
   * @param blk_n number of block we are getting state from
   * @return code at a given address.
   */
  bytes getCode(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const;

  /**
   * @brief Executes a new message call immediately without creating a transaction on the block chain. That means that
   * state would be reverted and not saved anywhere
   * @param trx state_api::EVMTransaction
   * @param blk_n EthBlockNumber number of block we are getting state from
   * @return state_api::ExecutionResult
   */
  state_api::ExecutionResult call(state_api::EVMTransaction const& trx, std::optional<EthBlockNumber> blk_n = {}) const;

  /**
   * @brief Trace execution of a new message call immediately without creating a transactions on the block chain. That
   * means that state would be reverted and not saved anywhere
   * @param trxs std::vector<state_api::EVMTransaction> vector of transaction to trace
   * @param blk_n EthBlockNumber number of block we are getting state from
   * @return std::string
   */
  std::string trace(std::vector<state_api::EVMTransaction> trx, EthBlockNumber blk_n,
                    std::optional<state_api::Tracing> params = {}) const;

  /**
   * @brief total count of eligible votes are in DPOS precompiled contract
   * @param blk_num EthBlockNumber number of block we are getting state from
   * @return total count of eligible votes
   */
  uint64_t dposEligibleTotalVoteCount(EthBlockNumber blk_num) const;

  /**
   * @brief total count of eligible votes account has in DPOS precompiled contract
   * @param blk_num EthBlockNumber number of block we are getting state from
   * @param addr account address
   * @return address eligible votes count
   */
  uint64_t dposEligibleVoteCount(EthBlockNumber blk_num, addr_t const& addr) const;

  /**
   * @brief method to check if address have enough votes to participate in consensus
   * @param blk_num EthBlockNumber number of block we are getting state from
   * @param addr account address
   * @return is address eligible
   */
  bool dposIsEligible(EthBlockNumber blk_num, addr_t const& addr) const;

  /**
   * @brief Get the vrf key object from DPOS state
   * @param addr account address
   * @param blk_n number of block we are getting state from
   * @return vrf_wrapper::vrf_pk_t
   */
  vrf_wrapper::vrf_pk_t dposGetVrfKey(EthBlockNumber blk_n, const addr_t& addr) const;

  /**
   * @brief Prune state db for all blocks older than blk_n
   * @param blk_n number of block we are getting state from
   */
  void prune(EthBlockNumber blk_n);

  /**
   * @brief Wait until next block is finalized
   */
  void waitForFinalized();

  std::vector<state_api::ValidatorStake> dposValidatorsTotalStakes(EthBlockNumber blk_num) const;

  uint256_t dposTotalAmountDelegated(EthBlockNumber blk_num) const;

  /**
   * @param blk_num
   * @return vector of validators vote counts for provided blk_num
   */
  std::vector<state_api::ValidatorVoteCount> dposValidatorsVoteCounts(EthBlockNumber blk_num) const;

  /**
   * @param blk_num
   * @return yield
   */
  uint64_t dposYield(EthBlockNumber blk_num) const;

  /**
   * @param blk_num
   * @return total supply
   */
  u256 dposTotalSupply(EthBlockNumber blk_num) const;

  /**
   * @param blk_num
   * @return bridge root
   */
  h256 getBridgeRoot(EthBlockNumber blk_num) const;

  /**
   * @param blk_num
   * @return bridge epoch
   */
  h256 getBridgeEpoch(EthBlockNumber blk_num) const;
  // TODO move out of here:

  std::pair<val_t, bool> getBalance(addr_t const& addr) const;
  SharedTransaction makeBridgeFinalizationTransaction();
  bool isNeedToFinalize(EthBlockNumber blk_num) const;
  std::vector<SharedTransaction> makeSystemTransactions(PbftPeriod blk_num);
  std::shared_ptr<const FinalizationResult> finalize_(PeriodData&& new_blk,
                                                      std::vector<h256>&& finalized_dag_blk_hashes,
                                                      std::shared_ptr<DagBlock>&& anchor);
  std::shared_ptr<BlockHeader> appendBlock(DB::Batch& batch, const addr_t& author, uint64_t timestamp,
                                           uint64_t gas_limit, const h256& state_root, u256 total_reward,
                                           const SharedTransactions& transactions = {},
                                           const TransactionReceipts& receipts = {}, const bytes& extra_data = {});

 private:
  std::shared_ptr<TransactionHashes> getTransactionHashes(std::optional<EthBlockNumber> n = {}) const;
  const SharedTransactions getTransactions(std::optional<EthBlockNumber> n = {}) const;
  std::shared_ptr<const BlockHeader> getBlockHeader(EthBlockNumber n) const;
  std::optional<h256> getBlockHash(EthBlockNumber n) const;
  EthBlockNumber lastIfAbsent(const std::optional<EthBlockNumber>& client_blk_n) const;
  static state_api::EVMTransaction toEvmTransaction(const SharedTransaction& trx);
  static void appendEvmTransactions(std::vector<state_api::EVMTransaction>& evm_trxs, const SharedTransactions& trxs);
  BlocksBlooms blockBlooms(const h256& chunk_id) const;
  static h256 blockBloomsChunkId(EthBlockNumber level, EthBlockNumber index);
  std::vector<EthBlockNumber> withBlockBloom(const LogBloom& b, EthBlockNumber from, EthBlockNumber to,
                                             EthBlockNumber level, EthBlockNumber index) const;

 private:
  std::shared_ptr<DB> db_;
  const uint64_t kBlockGasLimit;
  StateAPI state_api_;
  const bool kLightNode = false;
  const uint32_t kMaxLevelsPerPeriod;
  rewards::Stats rewards_;

  // It is not prepared to use more then 1 thread. Examine it if you want to change threads count
  boost::asio::thread_pool executor_thread_{1};

  std::atomic<uint64_t> num_executed_dag_blk_ = 0;
  std::atomic<uint64_t> num_executed_trx_ = 0;

  EthBlockNumber delegation_delay_;

  ValueByBlockCache<std::shared_ptr<const BlockHeader>> block_headers_cache_;
  ValueByBlockCache<std::optional<const h256>> block_hashes_cache_;
  ValueByBlockCache<const SharedTransactions> transactions_cache_;
  ValueByBlockCache<std::shared_ptr<const TransactionHashes>> transaction_hashes_cache_;
  MapByBlockCache<addr_t, std::optional<const state_api::Account>> accounts_cache_;

  ValueByBlockCache<uint64_t> total_vote_count_cache_;
  MapByBlockCache<addr_t, uint64_t> dpos_vote_count_cache_;
  MapByBlockCache<addr_t, uint64_t> dpos_is_eligible_cache_;

  std::condition_variable finalized_cv_;
  std::mutex finalized_mtx_;

  std::atomic<EthBlockNumber> last_block_number_;

  const HardforksConfig& kHardforksConfig;
  LOG_OBJECTS_DEFINE
};

/** @} */

}  // namespace taraxa::final_chain
