#pragma once

#include "final_chain/cache.hpp"
#include "final_chain/final_chain.hpp"
#include "rewards/rewards_stats.hpp"

namespace taraxa::final_chain {

class FinalChainImpl final : public FinalChain {
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

 public:
  FinalChainImpl(const std::shared_ptr<DB>& db, const taraxa::FullNodeConfig& config, const addr_t& node_addr);

  void stop() override;
  std::future<std::shared_ptr<const FinalizationResult>> finalize(
      PeriodData&& new_blk, std::vector<h256>&& finalized_dag_blk_hashes,
      std::shared_ptr<DagBlock>&& anchor = nullptr) override;
  EthBlockNumber delegation_delay() const override;
  SharedTransaction make_bridge_finalization_transaction();
  bool isNeedToFinalize(EthBlockNumber blk_num) const;
  std::vector<SharedTransaction> makeSystemTransactions(PbftPeriod blk_num);
  std::shared_ptr<const FinalizationResult> finalize_(PeriodData&& new_blk,
                                                      std::vector<h256>&& finalized_dag_blk_hashes,
                                                      std::shared_ptr<DagBlock>&& anchor);
  void prune(EthBlockNumber blk_n) override;
  std::shared_ptr<BlockHeader> append_block(DB::Batch& batch, const addr_t& author, uint64_t timestamp,
                                            uint64_t gas_limit, const h256& state_root, u256 total_reward,
                                            const SharedTransactions& transactions = {},
                                            const TransactionReceipts& receipts = {}, const bytes& extra_data = {});
  EthBlockNumber last_block_number() const override;
  std::optional<EthBlockNumber> block_number(const h256& h) const override;
  std::optional<h256> block_hash(std::optional<EthBlockNumber> n = {}) const override;
  std::shared_ptr<const BlockHeader> block_header(std::optional<EthBlockNumber> n = {}) const override;
  std::optional<TransactionLocation> transaction_location(const h256& trx_hash) const override;
  std::optional<TransactionReceipt> transaction_receipt(const h256& trx_h) const override;
  uint64_t transactionCount(std::optional<EthBlockNumber> n = {}) const override;
  std::shared_ptr<const TransactionHashes> transaction_hashes(std::optional<EthBlockNumber> n = {}) const override;
  const SharedTransactions transactions(std::optional<EthBlockNumber> n = {}) const override;
  std::vector<EthBlockNumber> withBlockBloom(const LogBloom& b, EthBlockNumber from, EthBlockNumber to) const override;
  std::optional<state_api::Account> get_account(const addr_t& addr,
                                                std::optional<EthBlockNumber> blk_n = {}) const override;
  void update_state_config(const state_api::Config& new_config) override;
  h256 get_account_storage(const addr_t& addr, const u256& key,
                           std::optional<EthBlockNumber> blk_n = {}) const override;
  bytes get_code(const addr_t& addr, std::optional<EthBlockNumber> blk_n = {}) const override;
  state_api::ExecutionResult call(const state_api::EVMTransaction& trx,
                                  std::optional<EthBlockNumber> blk_n = {}) const override;
  std::string trace(std::vector<state_api::EVMTransaction> trxs, EthBlockNumber blk_n,
                    std::optional<state_api::Tracing> params = {}) const override;
  uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const override;
  uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, const addr_t& addr) const override;
  bool dpos_is_eligible(EthBlockNumber blk_num, const addr_t& addr) const override;
  vrf_wrapper::vrf_pk_t dpos_get_vrf_key(EthBlockNumber blk_n, const addr_t& addr) const override;
  std::vector<state_api::ValidatorStake> dpos_validators_total_stakes(EthBlockNumber blk_num) const override;
  virtual uint256_t dpos_total_amount_delegated(EthBlockNumber blk_num) const override;
  std::vector<state_api::ValidatorVoteCount> dpos_validators_vote_counts(EthBlockNumber blk_num) const override;
  void wait_for_finalized() override;
  uint64_t dpos_yield(EthBlockNumber blk_num) const override;
  u256 dpos_total_supply(EthBlockNumber blk_num) const override;
  h256 get_bridge_root(EthBlockNumber blk_num) const override;
  h256 get_bridge_epoch(EthBlockNumber blk_num) const override;

 private:
  std::shared_ptr<TransactionHashes> get_transaction_hashes(std::optional<EthBlockNumber> n = {}) const;
  const SharedTransactions get_transactions(std::optional<EthBlockNumber> n = {}) const;
  std::shared_ptr<const BlockHeader> get_block_header(EthBlockNumber n) const;
  std::optional<h256> get_block_hash(EthBlockNumber n) const;
  EthBlockNumber last_if_absent(const std::optional<EthBlockNumber>& client_blk_n) const;
  static state_api::EVMTransaction to_evm_transaction(const SharedTransaction& trx);
  static void append_evm_transactions(std::vector<state_api::EVMTransaction>& evm_trxs, const SharedTransactions& trxs);
  BlocksBlooms block_blooms(const h256& chunk_id) const;
  static h256 block_blooms_chunk_id(EthBlockNumber level, EthBlockNumber index);
  std::vector<EthBlockNumber> withBlockBloom(const LogBloom& b, EthBlockNumber from, EthBlockNumber to,
                                             EthBlockNumber level, EthBlockNumber index) const;
};

}  // namespace taraxa::final_chain