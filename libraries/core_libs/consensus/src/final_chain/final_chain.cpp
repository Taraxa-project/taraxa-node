#include "final_chain/final_chain.hpp"

#include <cstdint>
#include <string>

#include "common/constants.hpp"
#include "common/thread_pool.hpp"
#include "final_chain/cache.hpp"
#include "final_chain/rewards_stats.hpp"
#include "final_chain/trie_common.hpp"
#include "vote/vote.hpp"

namespace taraxa::final_chain {

class FinalChainImpl final : public FinalChain {
  std::shared_ptr<DB> db_;
  const uint32_t kCommitteeSize;
  const uint64_t kBlockGasLimit;
  StateAPI state_api_;
  const bool kLightNode = false;
  const uint64_t kLightNodeHistory = 0;
  const uint32_t kMaxLevelsPerPeriod;
  const uint64_t kLightNodePruneOffset = 0;

  // It is not prepared to use more then 1 thread. Examine it if you want to change threads count
  boost::asio::thread_pool executor_thread_{1};
  boost::asio::thread_pool prune_thread_{1};

  std::atomic<uint64_t> num_executed_dag_blk_ = 0;
  std::atomic<uint64_t> num_executed_trx_ = 0;

  rocksdb::WriteOptions const db_opts_w_ = [] {
    rocksdb::WriteOptions ret;
    ret.sync = true;
    return ret;
  }();
  EthBlockNumber delegation_delay_;

  ValueByBlockCache<std::shared_ptr<const BlockHeader>> block_headers_cache_;
  ValueByBlockCache<std::optional<const h256>> block_hashes_cache_;
  ValueByBlockCache<const SharedTransactions> transactions_cache_;
  ValueByBlockCache<std::shared_ptr<const TransactionHashes>> transaction_hashes_cache_;
  MapByBlockCache<addr_t, std::optional<const state_api::Account>> accounts_cache_;

  ValueByBlockCache<uint64_t> total_vote_count_cache_;
  MapByBlockCache<addr_t, uint64_t> dpos_vote_count_cache_;
  MapByBlockCache<addr_t, uint64_t> dpos_is_eligible_cache_;

  LOG_OBJECTS_DEFINE

 public:
  FinalChainImpl(const std::shared_ptr<DB>& db, const taraxa::FullNodeConfig& config, const addr_t& node_addr)
      : db_(db),
        kCommitteeSize(config.genesis.pbft.committee_size),
        kBlockGasLimit(config.genesis.pbft.gas_limit),
        state_api_([this](auto n) { return block_hash(n).value_or(ZeroHash()); },  //
                   config.genesis.state, config.opts_final_chain,
                   {
                       db->stateDbStoragePath().string(),
                   }),
        kLightNode(config.is_light_node),
        kLightNodeHistory(config.light_node_history),
        kMaxLevelsPerPeriod(config.max_levels_per_period),
        // This will provide a speific random offset based on node address for each node to prevent all light nodes
        // performing prune at the same block height
        kLightNodePruneOffset((*reinterpret_cast<uint32_t*>(node_addr.asBytes().data())) %
                              std::max(config.light_node_history, (uint64_t)1)),
        block_headers_cache_(config.final_chain_cache_in_blocks,
                             [this](uint64_t blk) { return get_block_header(blk); }),
        block_hashes_cache_(config.final_chain_cache_in_blocks, [this](uint64_t blk) { return get_block_hash(blk); }),
        transactions_cache_(config.final_chain_cache_in_blocks, [this](uint64_t blk) { return get_transactions(blk); }),
        transaction_hashes_cache_(config.final_chain_cache_in_blocks,
                                  [this](uint64_t blk) { return get_transaction_hashes(blk); }),
        accounts_cache_(config.final_chain_cache_in_blocks,
                        [this](uint64_t blk, const addr_t& addr) { return state_api_.get_account(blk, addr); }),
        total_vote_count_cache_(config.final_chain_cache_in_blocks,
                                [this](uint64_t blk) { return state_api_.dpos_eligible_total_vote_count(blk); }),
        dpos_vote_count_cache_(
            config.final_chain_cache_in_blocks,
            [this](uint64_t blk, const addr_t& addr) { return state_api_.dpos_eligible_vote_count(blk, addr); }),
        dpos_is_eligible_cache_(config.final_chain_cache_in_blocks, [this](uint64_t blk, const addr_t& addr) {
          return state_api_.dpos_is_eligible(blk, addr);
        }) {
    LOG_OBJECTS_CREATE("EXECUTOR");
    num_executed_dag_blk_ = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
    num_executed_trx_ = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
    auto state_db_descriptor = state_api_.get_last_committed_state_descriptor();
    auto last_blk_num = db_->lookup_int<EthBlockNumber>(DBMetaKeys::LAST_NUMBER, DB::Columns::final_chain_meta);
    // If we don't have genesis block in db then create and push it
    if (!last_blk_num) [[unlikely]] {
      auto batch = db_->createWriteBatch();
      auto header = append_block(batch, addr_t(), config.genesis.dag_genesis_block.getTimestamp(), kBlockGasLimit,
                                 state_db_descriptor.state_root, u256(0));

      block_headers_cache_.append(header->number, header);
      db_->commitWriteBatch(batch, db_opts_w_);
    } else {
      // We need to recover latest changes as there was shutdown inside finalize function
      if (*last_blk_num != state_db_descriptor.blk_num) [[unlikely]] {
        auto batch = db_->createWriteBatch();
        for (auto block_n = *last_blk_num; block_n != state_db_descriptor.blk_num; --block_n) {
          auto raw_period_data = db_->getPeriodDataRaw(block_n);
          assert(raw_period_data.size() > 0);

          const PeriodData period_data(std::move(raw_period_data));
          if (period_data.transactions.size()) {
            num_executed_dag_blk_ -= period_data.dag_blocks.size();
            num_executed_trx_ -= period_data.transactions.size();
          }
        }
        db_->insert(batch, DB::Columns::status, StatusDbField::ExecutedBlkCount, num_executed_dag_blk_.load());
        db_->insert(batch, DB::Columns::status, StatusDbField::ExecutedTrxCount, num_executed_trx_.load());
        db_->insert(batch, DB::Columns::final_chain_meta, DBMetaKeys::LAST_NUMBER, state_db_descriptor.blk_num);
        db_->commitWriteBatch(batch, db_opts_w_);
        last_blk_num = state_db_descriptor.blk_num;
      }

      int64_t start = 0;
      if (*last_blk_num > 5) {
        start = *last_blk_num - 5;
      }
      for (uint64_t num = start; num <= *last_blk_num; ++num) {
        block_headers_cache_.get(num);
      }
    }

    delegation_delay_ = config.genesis.state.dpos.delegation_delay;
  }

  void stop() override {
    executor_thread_.join();
    prune_thread_.join();
  }

  std::future<std::shared_ptr<const FinalizationResult>> finalize(
      PeriodData&& new_blk, std::vector<h256>&& finalized_dag_blk_hashes,
      std::shared_ptr<DagBlock>&& anchor = nullptr) override {
    auto p = std::make_shared<std::promise<std::shared_ptr<const FinalizationResult>>>();
    boost::asio::post(executor_thread_, [this, new_blk = std::move(new_blk),
                                         finalized_dag_blk_hashes = std::move(finalized_dag_blk_hashes),
                                         anchor_block = std::move(anchor), p]() mutable {
      p->set_value(finalize_(std::move(new_blk), std::move(finalized_dag_blk_hashes), std::move(anchor_block)));
    });
    return p->get_future();
  }

  EthBlockNumber delegation_delay() const override { return delegation_delay_; }

  std::shared_ptr<const FinalizationResult> finalize_(PeriodData&& new_blk,
                                                      std::vector<h256>&& finalized_dag_blk_hashes,
                                                      std::shared_ptr<DagBlock>&& anchor) {
    auto batch = db_->createWriteBatch();

    RewardsStats rewards_stats;
    uint64_t dpos_vote_count = kCommitteeSize;
    // Block zero
    if (!new_blk.previous_block_cert_votes.empty()) [[unlikely]] {
      dpos_vote_count = dpos_eligible_total_vote_count(new_blk.previous_block_cert_votes[0]->getPeriod() - 1);
    }
    // returns list of validators for new_blk.transactions
    const std::vector<addr_t> txs_validators = rewards_stats.processStats(new_blk, dpos_vote_count, kCommitteeSize);

    block_applying_emitter_.emit(block_header()->number + 1);

    /*
    // Any dag block producer producing duplicate dag blocks on same level should be slashed


    std::map<std::pair<addr_t, uint64_t>, uint32_t> dag_blocks_per_addr_and_level;
    std::unordered_map<addr_t, uint32_t> duplicate_dag_blocks_count;

    for (const auto& block : new_blk.dag_blocks) {
      dag_blocks_per_addr_and_level[{block.getSender(), block.getLevel()}]++;
    }

    for (const auto& it : dag_blocks_per_addr_and_level) {
      if (it.second > 1) {
        duplicate_dag_blocks_count[it.first.first] += it.second - 1;
      }
    } */

    auto const& [exec_results, state_root, total_reward] =
        state_api_.transition_state({new_blk.pbft_blk->getBeneficiary(), kBlockGasLimit,
                                     new_blk.pbft_blk->getTimestamp(), BlockHeader::difficulty()},
                                    to_state_api_transactions(new_blk.transactions), txs_validators, {}, rewards_stats);

    TransactionReceipts receipts;
    receipts.reserve(exec_results.size());
    gas_t cumulative_gas_used = 0;
    for (auto const& r : exec_results) {
      LogEntries logs;
      logs.reserve(r.logs.size());
      std::transform(r.logs.cbegin(), r.logs.cend(), std::back_inserter(logs), [](const auto& l) {
        return LogEntry{l.address, l.topics, l.data};
      });
      receipts.emplace_back(TransactionReceipt{
          r.code_err.empty() && r.consensus_err.empty(),
          r.gas_used,
          cumulative_gas_used += r.gas_used,
          std::move(logs),
          r.new_contract_addr ? std::optional(r.new_contract_addr) : std::nullopt,
      });
    }
    auto blk_header = append_block(batch, new_blk.pbft_blk->getBeneficiary(), new_blk.pbft_blk->getTimestamp(),
                                   kBlockGasLimit, state_root, total_reward, new_blk.transactions, receipts);
    // Update number of executed DAG blocks and transactions
    auto num_executed_dag_blk = num_executed_dag_blk_ + finalized_dag_blk_hashes.size();
    auto num_executed_trx = num_executed_trx_ + new_blk.transactions.size();
    if (!finalized_dag_blk_hashes.empty()) {
      db_->insert(batch, DB::Columns::status, StatusDbField::ExecutedBlkCount, num_executed_dag_blk);
      db_->insert(batch, DB::Columns::status, StatusDbField::ExecutedTrxCount, num_executed_trx);
      LOG(log_nf_) << "Executed dag blocks #" << num_executed_dag_blk_ - finalized_dag_blk_hashes.size() << "-"
                   << num_executed_dag_blk_ - 1 << " , Transactions count: " << new_blk.transactions.size();
    }

    //// Update DAG lvl mapping
    if (anchor) {
      db_->addProposalPeriodDagLevelsMapToBatch(anchor->getLevel() + kMaxLevelsPerPeriod, new_blk.pbft_blk->getPeriod(),
                                                batch);
    }
    ////

    auto result = std::make_shared<FinalizationResult>(FinalizationResult{
        {
            new_blk.pbft_blk->getBeneficiary(),
            new_blk.pbft_blk->getTimestamp(),
            std::move(finalized_dag_blk_hashes),
            new_blk.pbft_blk->getBlockHash(),
        },
        blk_header,
        std::move(new_blk.transactions),
        std::move(receipts),
    });

    db_->commitWriteBatch(batch, db_opts_w_);
    state_api_.transition_state_commit();

    num_executed_dag_blk_ = num_executed_dag_blk;
    num_executed_trx_ = num_executed_trx;
    block_headers_cache_.append(blk_header->number, blk_header);
    block_finalized_emitter_.emit(result);
    LOG(log_nf_) << " successful finalize block " << result->hash << " with number " << blk_header->number;

    // Creates snapshot if needed
    if (db_->createSnapshot(blk_header->number)) {
      state_api_.create_snapshot(blk_header->number);
    }

    if (kLightNode) {
      // Actual history size will be between 100% and 105% of light_node_history_ to avoid deleting on every period
      if ((((blk_header->number + kLightNodePruneOffset) % (std::max(kLightNodeHistory / 20, (uint64_t)1)) == 0)) &&
          blk_header->number > kLightNodeHistory) {
        prune(blk_header->number - kLightNodeHistory);
      }
    }
    return result;
  }

  void prune(EthBlockNumber blk_n) override {
    const auto last_block_to_keep = get_block_header(blk_n);
    if (last_block_to_keep) {
      std::vector<dev::h256> state_root_to_prune;
      LOG(log_nf_) << "Pruning data older than " << blk_n;
      auto block_to_prune = get_block_header(last_block_to_keep->number - 1);
      while (block_to_prune && block_to_prune->number > 0) {
        state_root_to_prune.push_back(block_to_prune->state_root);
        db_->remove(DB::Columns::final_chain_blk_by_number, block_to_prune->number);
        db_->remove(DB::Columns::final_chain_blk_hash_by_number, block_to_prune->number);
        db_->remove(DB::Columns::final_chain_blk_number_by_hash, block_to_prune->hash);
        block_to_prune = get_block_header(block_to_prune->number - 1);
      }

      db_->compactColumn(DB::Columns::final_chain_blk_by_number);
      db_->compactColumn(DB::Columns::final_chain_blk_hash_by_number);
      db_->compactColumn(DB::Columns::final_chain_blk_number_by_hash);

      boost::asio::post(
          prune_thread_,
          [this, to_keep = std::move(last_block_to_keep->state_root), to_prune = std::move(state_root_to_prune),
           number = last_block_to_keep->number]() { state_api_.prune(to_keep, to_prune, number); });
    }
  }

  std::shared_ptr<BlockHeader> append_block(DB::Batch& batch, const addr_t& author, uint64_t timestamp,
                                            uint64_t gas_limit, const h256& state_root, u256 total_reward,
                                            const SharedTransactions& transactions = {},
                                            const TransactionReceipts& receipts = {}) {
    auto blk_header_ptr = std::make_shared<BlockHeader>();
    auto& blk_header = *blk_header_ptr;
    auto last_block = block_header();
    blk_header.number = last_block ? last_block->number + 1 : 0;
    blk_header.parent_hash = last_block ? last_block->hash : h256();
    blk_header.author = author;
    blk_header.timestamp = timestamp;
    blk_header.state_root = state_root;
    blk_header.gas_used = receipts.empty() ? 0 : receipts.back().cumulative_gas_used;
    blk_header.gas_limit = gas_limit;
    blk_header.total_reward = total_reward;
    dev::BytesMap trxs_trie, receipts_trie;
    dev::RLPStream rlp_strm;
    for (size_t i(0); i < transactions.size(); ++i) {
      auto const& trx = transactions[i];
      auto i_rlp = util::rlp_enc(rlp_strm, i);
      trxs_trie[i_rlp] = trx->rlp();
      auto const& receipt = receipts[i];
      receipts_trie[i_rlp] = util::rlp_enc(rlp_strm, receipt);
      db_->insert(batch, DB::Columns::final_chain_receipt_by_trx_hash, trx->getHash(), rlp_strm.out());
      auto bloom = receipt.bloom();
      blk_header.log_bloom |= bloom;
    }
    blk_header.transactions_root = hash256(trxs_trie);
    blk_header.receipts_root = hash256(receipts_trie);
    rlp_strm.clear(), blk_header.ethereum_rlp(rlp_strm);
    blk_header.hash = dev::sha3(rlp_strm.out());
    db_->insert(batch, DB::Columns::final_chain_blk_by_number, blk_header.number, util::rlp_enc(rlp_strm, blk_header));
    auto log_bloom_for_index = blk_header.log_bloom;
    log_bloom_for_index.shiftBloom<3>(sha3(blk_header.author.ref()));
    for (uint64_t level = 0, index = blk_header.number; level < c_bloomIndexLevels;
         ++level, index /= c_bloomIndexSize) {
      auto chunk_id = block_blooms_chunk_id(level, index / c_bloomIndexSize);
      auto chunk_to_alter = block_blooms(chunk_id);
      chunk_to_alter[index % c_bloomIndexSize] |= log_bloom_for_index;
      db_->insert(batch, DB::Columns::final_chain_log_blooms_index, chunk_id, util::rlp_enc(rlp_strm, chunk_to_alter));
    }
    db_->insert(batch, DB::Columns::final_chain_transaction_hashes_by_blk_number, blk_header.number,
                dev::rlp(hashes_from_transactions(transactions)));
    db_->insert(batch, DB::Columns::final_chain_blk_hash_by_number, blk_header.number, blk_header.hash);
    db_->insert(batch, DB::Columns::final_chain_blk_number_by_hash, blk_header.hash, blk_header.number);
    db_->insert(batch, DB::Columns::final_chain_meta, DBMetaKeys::LAST_NUMBER, blk_header.number);

    return blk_header_ptr;
  }

  EthBlockNumber last_block_number() const override { return block_headers_cache_.last()->number; }

  std::optional<EthBlockNumber> block_number(h256 const& h) const override {
    return db_->lookup_int<EthBlockNumber>(h, DB::Columns::final_chain_blk_number_by_hash);
  }

  std::optional<h256> block_hash(std::optional<EthBlockNumber> n = {}) const override {
    return block_hashes_cache_.get(last_if_absent(n));
  }

  std::shared_ptr<const BlockHeader> block_header(std::optional<EthBlockNumber> n = {}) const override {
    if (!n) {
      return block_headers_cache_.last();
    }
    return block_headers_cache_.get(*n);
  }

  std::optional<TransactionLocation> transaction_location(const h256& trx_hash) const override {
    const auto period = db_->getTransactionPeriod(trx_hash);
    if (!period) {
      return {};
    }
    return TransactionLocation{period->first, period->second};
  }

  std::optional<TransactionReceipt> transaction_receipt(h256 const& trx_h) const override {
    auto raw = db_->lookup(trx_h, DB::Columns::final_chain_receipt_by_trx_hash);
    if (raw.empty()) {
      return {};
    }
    TransactionReceipt ret;
    ret.rlp(dev::RLP(raw));
    return ret;
  }

  uint64_t transactionCount(std::optional<EthBlockNumber> n = {}) const override {
    return db_->getTransactionCount(last_if_absent(n));
  }

  std::shared_ptr<const TransactionHashes> transaction_hashes(std::optional<EthBlockNumber> n = {}) const override {
    return transaction_hashes_cache_.get(last_if_absent(n));
  }

  const SharedTransactions transactions(std::optional<EthBlockNumber> n = {}) const override {
    return transactions_cache_.get(last_if_absent(n));
  }

  std::vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from, EthBlockNumber to) const override {
    std::vector<EthBlockNumber> ret;
    // start from the top-level
    auto u = int_pow(c_bloomIndexSize, c_bloomIndexLevels);
    // run through each of the top-level blocks
    for (EthBlockNumber index = from / u; index <= (to + u - 1) / u; ++index) {
      dev::operator+=(ret, withBlockBloom(b, from, to, c_bloomIndexLevels - 1, index));
    }
    return ret;
  }

  std::optional<state_api::Account> get_account(addr_t const& addr,
                                                std::optional<EthBlockNumber> blk_n = {}) const override {
    return accounts_cache_.get(last_if_absent(blk_n), addr);
  }

  void update_state_config(const state_api::Config& new_config) override {
    delegation_delay_ = new_config.dpos.delegation_delay;
    state_api_.update_state_config(new_config);
  }

  u256 get_account_storage(addr_t const& addr, u256 const& key,
                           std::optional<EthBlockNumber> blk_n = {}) const override {
    return state_api_.get_account_storage(last_if_absent(blk_n), addr, key);
  }

  bytes get_code(addr_t const& addr, std::optional<EthBlockNumber> blk_n = {}) const override {
    return state_api_.get_code_by_address(last_if_absent(blk_n), addr);
  }

  state_api::ExecutionResult call(state_api::EVMTransaction const& trx,
                                  std::optional<EthBlockNumber> blk_n = {}) const override {
    auto const blk_header = block_header(last_if_absent(blk_n));
    return state_api_.dry_run_transaction(blk_header->number,
                                          {
                                              blk_header->author,
                                              blk_header->gas_limit,
                                              blk_header->timestamp,
                                              BlockHeader::difficulty(),
                                          },
                                          trx);
  }

  std::string trace_trx(const state_api::EVMTransaction& trx, EthBlockNumber blk_n,
                        std::optional<state_api::Tracing> params = {}) const override {
    const auto blk_header = block_header(last_if_absent(blk_n));
    return dev::asString(state_api_.trace_transaction(blk_header->number,
                                                      {
                                                          blk_header->author,
                                                          blk_header->gas_limit,
                                                          blk_header->timestamp,
                                                          BlockHeader::difficulty(),
                                                      },
                                                      trx, params));
  }

  uint64_t dpos_eligible_total_vote_count(EthBlockNumber blk_num) const override {
    return total_vote_count_cache_.get(blk_num);
  }

  uint64_t dpos_eligible_vote_count(EthBlockNumber blk_num, addr_t const& addr) const override {
    return dpos_vote_count_cache_.get(blk_num, addr);
  }

  bool dpos_is_eligible(EthBlockNumber blk_num, addr_t const& addr) const override {
    return dpos_is_eligible_cache_.get(blk_num, addr);
  }

  vrf_wrapper::vrf_pk_t dpos_get_vrf_key(EthBlockNumber blk_n, const addr_t& addr) const override {
    return state_api_.dpos_get_vrf_key(blk_n, addr);
  }

 private:
  std::shared_ptr<TransactionHashes> get_transaction_hashes(std::optional<EthBlockNumber> n = {}) const {
    auto res = db_->lookup(last_if_absent(n), DB::Columns::final_chain_transaction_hashes_by_blk_number);

    return std::make_shared<TransactionHashes>(util::rlp_dec<TransactionHashes>(dev::RLP(res)));
  }

  const SharedTransactions get_transactions(std::optional<EthBlockNumber> n = {}) const {
    SharedTransactions ret;
    auto hashes = transaction_hashes(n);
    ret.reserve(hashes->size());
    for (size_t i = 0; i < ret.capacity(); ++i) {
      auto trx = db_->getTransaction(hashes->at(i));
      assert(trx);
      ret.emplace_back(trx);
    }
    return ret;
  }

  std::shared_ptr<const BlockHeader> get_block_header(EthBlockNumber n) const {
    if (auto raw = db_->lookup(n, DB::Columns::final_chain_blk_by_number); !raw.empty()) {
      auto ret = std::make_shared<BlockHeader>();
      ret->rlp(dev::RLP(raw));
      return ret;
    }
    return {};
  }

  std::optional<h256> get_block_hash(EthBlockNumber n) const {
    auto raw = db_->lookup(n, DB::Columns::final_chain_blk_hash_by_number);
    if (raw.empty()) {
      return {};
    }
    return h256(raw, h256::FromBinary);
  }

  EthBlockNumber last_if_absent(std::optional<EthBlockNumber> const& client_blk_n) const {
    return client_blk_n ? *client_blk_n : last_block_number();
  }

  static util::RangeView<state_api::EVMTransaction> to_state_api_transactions(SharedTransactions const& trxs) {
    return util::make_range_view(trxs).map([](auto const& trx) {
      return state_api::EVMTransaction{
          trx->getSender(), trx->getGasPrice(), trx->getReceiver(), trx->getNonce(),
          trx->getValue(),  trx->getGas(),      trx->getData(),
      };
    });
  }

  BlocksBlooms block_blooms(h256 const& chunk_id) const {
    if (auto raw = db_->lookup(chunk_id, DB::Columns::final_chain_log_blooms_index); !raw.empty()) {
      return dev::RLP(raw).toArray<LogBloom, c_bloomIndexSize>();
    }
    return {};
  }

  static h256 block_blooms_chunk_id(EthBlockNumber level, EthBlockNumber index) { return h256(index * 0xff + level); }

  std::vector<EthBlockNumber> withBlockBloom(LogBloom const& b, EthBlockNumber from, EthBlockNumber to,
                                             EthBlockNumber level, EthBlockNumber index) const {
    std::vector<EthBlockNumber> ret;
    auto uCourse = int_pow(c_bloomIndexSize, level + 1);
    auto uFine = int_pow(c_bloomIndexSize, level);
    auto obegin = index == from / uCourse ? from / uFine % c_bloomIndexSize : 0;
    auto oend = index == to / uCourse ? (to / uFine) % c_bloomIndexSize + 1 : c_bloomIndexSize;
    auto bb = block_blooms(block_blooms_chunk_id(level, index));
    for (auto o = obegin; o < oend; ++o) {
      if (bb[o].contains(b)) {
        // This level has something like what we want.
        if (level > 0) {
          dev::operator+=(ret, withBlockBloom(b, from, to, level - 1, o + index * c_bloomIndexSize));
        } else {
          ret.push_back(o + index * c_bloomIndexSize);
        }
      }
    }
    return ret;
  }
};

std::shared_ptr<FinalChain> NewFinalChain(const std::shared_ptr<DB>& db, const taraxa::FullNodeConfig& config,
                                          const addr_t& node_addr) {
  return make_shared<FinalChainImpl>(db, config, node_addr);
}

}  // namespace taraxa::final_chain
