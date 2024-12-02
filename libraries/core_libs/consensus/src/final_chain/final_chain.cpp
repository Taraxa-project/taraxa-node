#include "final_chain/final_chain.hpp"

#include "common/encoding_solidity.hpp"
#include "common/util.hpp"
#include "final_chain/state_api_data.hpp"
#include "final_chain/trie_common.hpp"
#include "pbft/pbft_block.hpp"
#include "transaction/system_transaction.hpp"

namespace taraxa::final_chain {
FinalChain::FinalChain(const std::shared_ptr<DbStorage>& db, const taraxa::FullNodeConfig& config,
                       const addr_t& node_addr)
    : db_(db),
      kBlockGasLimit(config.genesis.pbft.gas_limit),
      state_api_([this](auto n) { return blockHash(n).value_or(ZeroHash()); },  //
                 config.genesis.state, config.opts_final_chain,
                 {
                     db->stateDbStoragePath().string(),
                 }),
      kLightNode(config.is_light_node),
      kMaxLevelsPerPeriod(config.max_levels_per_period),
      rewards_(
          config.genesis.pbft.committee_size, config.genesis.state.hardforks, db_,
          [this](EthBlockNumber n) { return dposEligibleTotalVoteCount(n); },
          state_api_.get_last_committed_state_descriptor().blk_num),
      block_headers_cache_(config.final_chain_cache_in_blocks, [this](uint64_t blk) { return getBlockHeader(blk); }),
      block_hashes_cache_(config.final_chain_cache_in_blocks, [this](uint64_t blk) { return getBlockHash(blk); }),
      transactions_cache_(config.final_chain_cache_in_blocks, [this](uint64_t blk) { return getTransactions(blk); }),
      transaction_hashes_cache_(config.final_chain_cache_in_blocks,
                                [this](uint64_t blk) { return getTransactionHashes(blk); }),
      accounts_cache_(config.final_chain_cache_in_blocks,
                      [this](uint64_t blk, const addr_t& addr) { return state_api_.get_account(blk, addr); }),
      total_vote_count_cache_(config.final_chain_cache_in_blocks,
                              [this](uint64_t blk) { return state_api_.dpos_eligible_total_vote_count(blk); }),
      dpos_vote_count_cache_(
          config.final_chain_cache_in_blocks,
          [this](uint64_t blk, const addr_t& addr) { return state_api_.dpos_eligible_vote_count(blk, addr); }),
      dpos_is_eligible_cache_(
          config.final_chain_cache_in_blocks,
          [this](uint64_t blk, const addr_t& addr) { return state_api_.dpos_is_eligible(blk, addr); }),
      kConfig(config) {
  LOG_OBJECTS_CREATE("EXECUTOR");
  num_executed_dag_blk_ = db_->getStatusField(taraxa::StatusDbField::ExecutedBlkCount);
  num_executed_trx_ = db_->getStatusField(taraxa::StatusDbField::ExecutedTrxCount);
  auto state_db_descriptor = state_api_.get_last_committed_state_descriptor();
  auto last_blk_num = db_->lookup_int<EthBlockNumber>(DBMetaKeys::LAST_NUMBER, DbStorage::Columns::final_chain_meta);
  // If we don't have genesis block in db then create and push it
  if (!last_blk_num) [[unlikely]] {
    auto batch = db_->createWriteBatch();
    auto header = makeGenesisHeader(state_db_descriptor.state_root);
    appendBlock(batch, header, {}, {});

    block_headers_cache_.append(header->number, header);
    last_block_number_ = header->number;
    db_->commitWriteBatch(batch);
  } else {
    // We need to recover latest changes as there was shutdown inside finalize function
    if (*last_blk_num != state_db_descriptor.blk_num) [[unlikely]] {
      auto batch = db_->createWriteBatch();
      for (auto block_n = *last_blk_num; block_n != state_db_descriptor.blk_num; --block_n) {
        auto period_data = db_->getPeriodData(block_n);
        assert(period_data.has_value());

        if (period_data->transactions.size()) {
          num_executed_dag_blk_ -= period_data->dag_blocks.size();
          num_executed_trx_ -= period_data->transactions.size();
        }
        auto period_system_transactions = db_->getPeriodSystemTransactionsHashes(block_n);
        num_executed_trx_ -= period_system_transactions.size();
      }
      db_->insert(batch, DbStorage::Columns::status, StatusDbField::ExecutedBlkCount, num_executed_dag_blk_.load());
      db_->insert(batch, DbStorage::Columns::status, StatusDbField::ExecutedTrxCount, num_executed_trx_.load());
      db_->insert(batch, DbStorage::Columns::final_chain_meta, DBMetaKeys::LAST_NUMBER, state_db_descriptor.blk_num);
      db_->commitWriteBatch(batch);
      last_blk_num = state_db_descriptor.blk_num;
    }
    last_block_number_ = *last_blk_num;

    int64_t start = 0;
    if (*last_blk_num > 5) {
      start = *last_blk_num - 5;
    }
    for (uint64_t num = start; num <= *last_blk_num; ++num) {
      block_headers_cache_.get(num);
    }
  }

  delegation_delay_ = config.genesis.state.dpos.delegation_delay;
  const auto kPruneBlocksToKeep = kDagExpiryLevelLimit + kMaxLevelsPerPeriod + 1;
  if ((config.db_config.prune_state_db || kLightNode) && last_blk_num.has_value() &&
      *last_blk_num > kPruneBlocksToKeep) {
    LOG(log_si_) << "Pruning state db, this might take several minutes";
    prune(*last_blk_num - kPruneBlocksToKeep);
    LOG(log_si_) << "Pruning state db complete";
  }
}

void FinalChain::stop() { executor_thread_.join(); }

std::future<std::shared_ptr<const FinalizationResult>> FinalChain::finalize(
    PeriodData&& new_blk, std::vector<h256>&& finalized_dag_blk_hashes, std::shared_ptr<DagBlock>&& anchor) {
  auto p = std::make_shared<std::promise<std::shared_ptr<const FinalizationResult>>>();
  boost::asio::post(executor_thread_, [this, new_blk = std::move(new_blk),
                                       finalized_dag_blk_hashes = std::move(finalized_dag_blk_hashes),
                                       anchor_block = std::move(anchor), p]() mutable {
    p->set_value(finalize_(std::move(new_blk), std::move(finalized_dag_blk_hashes), std::move(anchor_block)));
    finalized_cv_.notify_one();
  });
  return p->get_future();
}

EthBlockNumber FinalChain::delegationDelay() const { return delegation_delay_; }

SharedTransaction FinalChain::makeBridgeFinalizationTransaction() {
  const static auto finalize_method = util::EncodingSolidity::packFunctionCall("finalizeEpoch()");
  auto account = getAccount(kTaraxaSystemAccount).value_or(state_api::ZeroAccount);

  auto trx = std::make_shared<SystemTransaction>(account.nonce, 0, 0, kBlockGasLimit, finalize_method,
                                                 kConfig.genesis.state.hardforks.ficus_hf.bridge_contract_address);
  return trx;
}

bool FinalChain::isNeedToFinalize(EthBlockNumber blk_num) const {
  const static auto get_bridge_root_method = util::EncodingSolidity::packFunctionCall("shouldFinalizeEpoch()");
  return u256(call(state_api::EVMTransaction{dev::ZeroAddress, 1,
                                             kConfig.genesis.state.hardforks.ficus_hf.bridge_contract_address,
                                             state_api::ZeroAccount.nonce, 0, 10000000, get_bridge_root_method},
                   blk_num)
                  .code_retval)
      .convert_to<bool>();
}

std::vector<SharedTransaction> FinalChain::makeSystemTransactions(PbftPeriod blk_num) {
  std::vector<SharedTransaction> system_transactions;
  // Make system transactions <delegationDelay()> blocks sooner than next pillar block period,
  // e.g.: if pillar block period is 100, this will return true for period 100 - delegationDelay() == 95, 195, 295,
  // etc...
  if (kConfig.genesis.state.hardforks.ficus_hf.isPillarBlockPeriod(blk_num + delegationDelay())) {
    if (const auto bridge_contract = getAccount(kConfig.genesis.state.hardforks.ficus_hf.bridge_contract_address);
        bridge_contract) {
      if (bridge_contract->code_size && isNeedToFinalize(blk_num - 1)) {
        auto finalize_trx = makeBridgeFinalizationTransaction();
        system_transactions.push_back(finalize_trx);
      }
    }
  }

  return system_transactions;
}

std::shared_ptr<const FinalizationResult> FinalChain::finalize_(PeriodData&& new_blk,
                                                                std::vector<h256>&& finalized_dag_blk_hashes,
                                                                std::shared_ptr<DagBlock>&& anchor) {
  auto batch = db_->createWriteBatch();

  block_applying_emitter_.emit(blockHeader()->number + 1);

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

  auto system_transactions = makeSystemTransactions(new_blk.pbft_blk->getPeriod());

  auto all_transactions = new_blk.transactions;
  all_transactions.insert(all_transactions.end(), system_transactions.begin(), system_transactions.end());
  std::vector<state_api::EVMTransaction> evm_trxs;
  appendEvmTransactions(evm_trxs, all_transactions);

  const auto& [exec_results] = state_api_.execute_transactions(
      {new_blk.pbft_blk->getBeneficiary(), kBlockGasLimit, new_blk.pbft_blk->getTimestamp(), BlockHeader::difficulty()},
      evm_trxs);
  TransactionReceipts receipts;
  receipts.reserve(exec_results.size());
  std::vector<gas_t> transactions_gas_used;
  transactions_gas_used.reserve(exec_results.size());

  gas_t cumulative_gas_used = 0;
  for (const auto& r : exec_results) {
    LogEntries logs;
    logs.reserve(r.logs.size());
    std::transform(r.logs.cbegin(), r.logs.cend(), std::back_inserter(logs), [](const auto& l) {
      return LogEntry{l.address, l.topics, l.data};
    });
    transactions_gas_used.push_back(r.gas_used);
    receipts.emplace_back(TransactionReceipt{
        r.code_err.empty() && r.consensus_err.empty(),
        r.gas_used,
        cumulative_gas_used += r.gas_used,
        std::move(logs),
        r.new_contract_addr ? std::optional(r.new_contract_addr) : std::nullopt,
    });
  }

  auto rewards_stats = rewards_.processStats(new_blk, transactions_gas_used, batch);
  const auto& [state_root, total_reward] = state_api_.distribute_rewards(rewards_stats);

  auto blk_header = appendBlock(batch, *new_blk.pbft_blk, state_root, total_reward, all_transactions, receipts);

  // Update number of executed DAG blocks and transactions
  auto num_executed_dag_blk = num_executed_dag_blk_ + finalized_dag_blk_hashes.size();
  auto num_executed_trx = num_executed_trx_ + all_transactions.size();
  if (!finalized_dag_blk_hashes.empty()) {
    db_->insert(batch, DbStorage::Columns::status, StatusDbField::ExecutedBlkCount, num_executed_dag_blk);
    db_->insert(batch, DbStorage::Columns::status, StatusDbField::ExecutedTrxCount, num_executed_trx);
    LOG(log_nf_) << "Executed dag blocks #" << num_executed_dag_blk_ - finalized_dag_blk_hashes.size() << "-"
                 << num_executed_dag_blk_ - 1 << " , Transactions count: " << all_transactions.size();
  }

  //// Update DAG lvl mapping
  if (anchor) {
    db_->addProposalPeriodDagLevelsMapToBatch(anchor->getLevel() + kMaxLevelsPerPeriod, new_blk.pbft_blk->getPeriod(),
                                              batch);
  }
  ////

  //// Commit system transactions
  if (!system_transactions.empty()) {
    db_->addPeriodSystemTransactions(batch, system_transactions, new_blk.pbft_blk->getPeriod());
    auto position = new_blk.transactions.size() + 1;
    for (const auto& trx : system_transactions) {
      db_->addSystemTransactionToBatch(batch, trx);
      db_->addTransactionLocationToBatch(batch, trx->getHash(), new_blk.pbft_blk->getPeriod(), position,
                                         true /*system_trx*/);
      position++;
    }
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
      std::move(all_transactions),
      std::move(receipts),
  });

  // Please do not change order of these three lines :)
  db_->commitWriteBatch(batch);
  state_api_.transition_state_commit();
  rewards_.clear(new_blk.pbft_blk->getPeriod());

  num_executed_dag_blk_ = num_executed_dag_blk;
  num_executed_trx_ = num_executed_trx;
  block_headers_cache_.append(blk_header->number, blk_header);
  last_block_number_ = blk_header->number;
  block_finalized_emitter_.emit(result);
  LOG(log_nf_) << " successful finalize block " << result->hash << " with number " << blk_header->number;

  // Creates snapshot if needed
  if (db_->createSnapshot(blk_header->number)) {
    state_api_.create_snapshot(blk_header->number);
  }

  return result;
}

void FinalChain::prune(EthBlockNumber blk_n) {
  LOG(log_nf_) << "Pruning data older than " << blk_n;
  auto last_block_to_keep = getBlockHeader(blk_n);
  if (last_block_to_keep) {
    auto block_to_keep = last_block_to_keep;
    std::vector<dev::h256> state_root_to_keep;
    while (block_to_keep) {
      state_root_to_keep.push_back(block_to_keep->state_root);
      block_to_keep = getBlockHeader(block_to_keep->number + 1);
    }
    auto block_to_prune = getBlockHeader(last_block_to_keep->number - 1);
    while (block_to_prune && block_to_prune->number > 0) {
      db_->remove(DbStorage::Columns::final_chain_blk_by_number, block_to_prune->number);
      db_->remove(DbStorage::Columns::final_chain_blk_hash_by_number, block_to_prune->number);
      db_->remove(DbStorage::Columns::final_chain_blk_number_by_hash, block_to_prune->hash);
      block_to_prune = getBlockHeader(block_to_prune->number - 1);
    }

    db_->compactColumn(DbStorage::Columns::final_chain_blk_by_number);
    db_->compactColumn(DbStorage::Columns::final_chain_blk_hash_by_number);
    db_->compactColumn(DbStorage::Columns::final_chain_blk_number_by_hash);

    state_api_.prune(state_root_to_keep, last_block_to_keep->number);
  }
}

std::shared_ptr<BlockHeader> FinalChain::appendBlock(Batch& batch, const PbftBlock& pbft_blk, const h256& state_root,
                                                     u256 total_reward, const SharedTransactions& transactions,
                                                     const TransactionReceipts& receipts) {
  auto header = std::make_shared<BlockHeader>();
  header->setFromPbft(pbft_blk);

  if (auto last_block = blockHeader(); last_block) {
    header->number = last_block->number + 1;
    header->parent_hash = last_block->hash;
  }
  if (!receipts.empty()) {
    header->gas_used = receipts.back().cumulative_gas_used;
  }
  header->state_root = state_root;
  header->total_reward = total_reward;
  header->gas_limit = kBlockGasLimit;

  return appendBlock(batch, std::move(header), transactions, receipts);
}

std::shared_ptr<BlockHeader> FinalChain::appendBlock(Batch& batch, std::shared_ptr<BlockHeader> header,
                                                     const SharedTransactions& transactions,
                                                     const TransactionReceipts& receipts) {
  dev::BytesMap trxs_trie, receipts_trie;
  dev::RLPStream rlp_strm;
  size_t trx_idx = 0;
  for (; trx_idx < transactions.size(); ++trx_idx) {
    const auto& trx = transactions[trx_idx];
    auto i_rlp = util::rlp_enc(rlp_strm, trx_idx);
    trxs_trie[i_rlp] = trx->rlp();

    const auto& receipt = receipts[trx_idx];
    receipts_trie[i_rlp] = util::rlp_enc(rlp_strm, receipt);
    db_->insert(batch, DbStorage::Columns::final_chain_receipt_by_trx_hash, trx->getHash(), rlp_strm.out());

    header->log_bloom |= receipt.bloom();
  }

  header->transactions_root = hash256(trxs_trie);
  header->receipts_root = hash256(receipts_trie);
  header->hash = dev::sha3(header->ethereumRlp());

  auto data = header->serializeForDB();
  db_->insert(batch, DbStorage::Columns::final_chain_blk_by_number, header->number, data);

  auto log_bloom_for_index = header->log_bloom;
  log_bloom_for_index.shiftBloom<3>(sha3(header->author.ref()));
  for (uint64_t level = 0, index = header->number; level < c_bloomIndexLevels; ++level, index /= c_bloomIndexSize) {
    auto chunk_id = blockBloomsChunkId(level, index / c_bloomIndexSize);
    auto chunk_to_alter = blockBlooms(chunk_id);
    chunk_to_alter[index % c_bloomIndexSize] |= log_bloom_for_index;
    db_->insert(batch, DbStorage::Columns::final_chain_log_blooms_index, chunk_id,
                util::rlp_enc(rlp_strm, chunk_to_alter));
  }
  db_->insert(batch, DbStorage::Columns::final_chain_blk_hash_by_number, header->number, header->hash);
  db_->insert(batch, DbStorage::Columns::final_chain_blk_number_by_hash, header->hash, header->number);
  db_->insert(batch, DbStorage::Columns::final_chain_meta, DBMetaKeys::LAST_NUMBER, header->number);

  return header;
}

EthBlockNumber FinalChain::lastBlockNumber() const { return last_block_number_; }

std::optional<EthBlockNumber> FinalChain::blockNumber(const h256& h) const {
  return db_->lookup_int<EthBlockNumber>(h, DbStorage::Columns::final_chain_blk_number_by_hash);
}

std::optional<h256> FinalChain::blockHash(std::optional<EthBlockNumber> n) const {
  return block_hashes_cache_.get(lastIfAbsent(n));
}

std::shared_ptr<const BlockHeader> FinalChain::blockHeader(std::optional<EthBlockNumber> n) const {
  if (!n) {
    return block_headers_cache_.last();
  }
  return block_headers_cache_.get(*n);
}

std::optional<TransactionLocation> FinalChain::transactionLocation(const h256& trx_hash) const {
  return db_->getTransactionLocation(trx_hash);
}

std::optional<TransactionReceipt> FinalChain::transactionReceipt(const h256& trx_h) const {
  auto raw = db_->lookup(trx_h, DbStorage::Columns::final_chain_receipt_by_trx_hash);
  if (raw.empty()) {
    return {};
  }
  TransactionReceipt ret;
  ret.rlp(dev::RLP(raw));
  return ret;
}

uint64_t FinalChain::transactionCount(std::optional<EthBlockNumber> n) const {
  return db_->getTransactionCount(lastIfAbsent(n));
}

std::shared_ptr<const TransactionHashes> FinalChain::transactionHashes(std::optional<EthBlockNumber> n) const {
  return transaction_hashes_cache_.get(lastIfAbsent(n));
}

const SharedTransactions FinalChain::transactions(std::optional<EthBlockNumber> n) const {
  return transactions_cache_.get(lastIfAbsent(n));
}

std::vector<EthBlockNumber> FinalChain::withBlockBloom(const LogBloom& b, EthBlockNumber from,
                                                       EthBlockNumber to) const {
  std::vector<EthBlockNumber> ret;
  // start from the top-level
  auto u = int_pow(c_bloomIndexSize, c_bloomIndexLevels);
  // run through each of the top-level blocks
  for (EthBlockNumber index = from / u; index <= (to + u - 1) / u; ++index) {
    dev::operator+=(ret, withBlockBloom(b, from, to, c_bloomIndexLevels - 1, index));
  }
  return ret;
}

std::optional<state_api::Account> FinalChain::getAccount(const addr_t& addr,
                                                         std::optional<EthBlockNumber> blk_n) const {
  return accounts_cache_.get(lastIfAbsent(blk_n), addr);
}

void FinalChain::updateStateConfig(const state_api::Config& new_config) {
  delegation_delay_ = new_config.dpos.delegation_delay;
  state_api_.update_state_config(new_config);
}

h256 FinalChain::getAccountStorage(const addr_t& addr, const u256& key, std::optional<EthBlockNumber> blk_n) const {
  return state_api_.get_account_storage(lastIfAbsent(blk_n), addr, key);
}

bytes FinalChain::getCode(const addr_t& addr, std::optional<EthBlockNumber> blk_n) const {
  return state_api_.get_code_by_address(lastIfAbsent(blk_n), addr);
}

state_api::ExecutionResult FinalChain::call(const state_api::EVMTransaction& trx,
                                            std::optional<EthBlockNumber> blk_n) const {
  auto const blk_header = blockHeader(lastIfAbsent(blk_n));
  if (!blk_header) {
    throw std::runtime_error("Future block");
  }
  return state_api_.dry_run_transaction(blk_header->number,
                                        {
                                            blk_header->author,
                                            blk_header->gas_limit,
                                            blk_header->timestamp,
                                            BlockHeader::difficulty(),
                                        },
                                        trx);
}

std::string FinalChain::trace(std::vector<state_api::EVMTransaction> state_trxs,
                              std::vector<state_api::EVMTransaction> trxs, EthBlockNumber blk_n,
                              std::optional<state_api::Tracing> params) const {
  const auto blk_header = blockHeader(lastIfAbsent(blk_n));
  if (!blk_header) {
    throw std::runtime_error("Future block");
  }
  return dev::asString(state_api_.trace(blk_header->number,
                                        {
                                            blk_header->author,
                                            blk_header->gas_limit,
                                            blk_header->timestamp,
                                            BlockHeader::difficulty(),
                                        },
                                        state_trxs, trxs, params));
}

uint64_t FinalChain::dposEligibleTotalVoteCount(EthBlockNumber blk_num) const {
  return total_vote_count_cache_.get(blk_num);
}

uint64_t FinalChain::dposEligibleVoteCount(EthBlockNumber blk_num, const addr_t& addr) const {
  return dpos_vote_count_cache_.get(blk_num, addr);
}

bool FinalChain::dposIsEligible(EthBlockNumber blk_num, const addr_t& addr) const {
  return dpos_is_eligible_cache_.get(blk_num, addr);
}

vrf_wrapper::vrf_pk_t FinalChain::dposGetVrfKey(EthBlockNumber blk_n, const addr_t& addr) const {
  return state_api_.dpos_get_vrf_key(blk_n, addr);
}

std::vector<state_api::ValidatorStake> FinalChain::dposValidatorsTotalStakes(EthBlockNumber blk_num) const {
  return state_api_.dpos_validators_total_stakes(blk_num);
}

uint256_t FinalChain::dposTotalAmountDelegated(EthBlockNumber blk_num) const {
  return state_api_.dpos_total_amount_delegated(blk_num);
}

std::vector<state_api::ValidatorVoteCount> FinalChain::dposValidatorsVoteCounts(EthBlockNumber blk_num) const {
  return state_api_.dpos_validators_vote_counts(blk_num);
}

void FinalChain::waitForFinalized() {
  std::unique_lock lck(finalized_mtx_);
  finalized_cv_.wait_for(lck, std::chrono::milliseconds(10));
}

uint64_t FinalChain::dposYield(EthBlockNumber blk_num) const { return state_api_.dpos_yield(blk_num); }

u256 FinalChain::dposTotalSupply(EthBlockNumber blk_num) const { return state_api_.dpos_total_supply(blk_num); }

h256 FinalChain::getBridgeRoot(EthBlockNumber blk_num) const {
  const static auto get_bridge_root_method = util::EncodingSolidity::packFunctionCall("getBridgeRoot()");
  return h256(call(state_api::EVMTransaction{dev::ZeroAddress, 1,
                                             kConfig.genesis.state.hardforks.ficus_hf.bridge_contract_address,
                                             state_api::ZeroAccount.nonce, 0, 10000000, get_bridge_root_method},
                   blk_num)
                  .code_retval);
}

h256 FinalChain::getBridgeEpoch(EthBlockNumber blk_num) const {
  const static auto getBridgeEpoch_method = util::EncodingSolidity::packFunctionCall("finalizedEpoch()");
  return h256(call(state_api::EVMTransaction{dev::ZeroAddress, 1,
                                             kConfig.genesis.state.hardforks.ficus_hf.bridge_contract_address,
                                             state_api::ZeroAccount.nonce, 0, 10000000, getBridgeEpoch_method},
                   blk_num)
                  .code_retval);
}

std::pair<val_t, bool> FinalChain::getBalance(addr_t const& addr) const {
  if (auto acc = getAccount(addr)) {
    return {acc->balance, true};
  }
  return {0, false};
}

std::shared_ptr<TransactionHashes> FinalChain::getTransactionHashes(std::optional<EthBlockNumber> n) const {
  const auto& trxs = db_->getPeriodTransactions(lastIfAbsent(n));
  auto ret = std::make_shared<TransactionHashes>();
  if (!trxs) {
    return ret;
  }
  ret->reserve(trxs->size());
  std::transform(trxs->cbegin(), trxs->cend(), std::back_inserter(*ret),
                 [](const auto& trx) { return trx->getHash(); });
  return ret;
}

const SharedTransactions FinalChain::getTransactions(std::optional<EthBlockNumber> n) const {
  if (auto trxs = db_->getPeriodTransactions(lastIfAbsent(n))) {
    return *trxs;
  }
  return {};
}

std::shared_ptr<BlockHeader> FinalChain::makeGenesisHeader(std::string&& raw_header) const {
  auto bh = std::make_shared<BlockHeader>(std::move(raw_header));
  bh->gas_limit = kConfig.genesis.pbft.gas_limit;
  bh->timestamp = kConfig.genesis.dag_genesis_block.getTimestamp();
  bh->hash = dev::sha3(bh->ethereumRlp());
  return bh;
}

std::shared_ptr<BlockHeader> FinalChain::makeGenesisHeader(const h256& state_root) const {
  auto header = std::make_shared<BlockHeader>();
  header->timestamp = kConfig.genesis.dag_genesis_block.getTimestamp();
  header->state_root = state_root;
  header->gas_limit = kConfig.genesis.pbft.gas_limit;
  header->hash = dev::sha3(header->ethereumRlp());
  return header;
}

std::shared_ptr<const BlockHeader> FinalChain::getBlockHeader(EthBlockNumber n) const {
  if (auto raw = db_->lookup(n, DbStorage::Columns::final_chain_blk_by_number); !raw.empty()) {
    if (n == 0) {
      return makeGenesisHeader(std::move(raw));
    }
    auto pbft = db_->getPbftBlock(n);
    // we should usually have a pbft block for a final chain block
    if (!pbft) {
      return {};
    }
    return std::make_shared<BlockHeader>(std::move(raw), *pbft, kBlockGasLimit);
  }
  return {};
}

std::optional<h256> FinalChain::finalChainHash(EthBlockNumber n) const {
  auto delay = delegationDelay();
  if (n <= delay) {
    // first delegation delay blocks will have zero hash
    return ZeroHash();
  }
  auto header = blockHeader(n - delay);
  if (!header) {
    return {};
  }

  if (kConfig.genesis.state.hardforks.isOnCornusHardfork(n)) {
    return header->hash;
  }
  return header->state_root;
}

std::optional<h256> FinalChain::getBlockHash(EthBlockNumber n) const {
  auto raw = db_->lookup(n, DbStorage::Columns::final_chain_blk_hash_by_number);
  if (raw.empty()) {
    return {};
  }
  return h256(raw, h256::FromBinary);
}

EthBlockNumber FinalChain::lastIfAbsent(const std::optional<EthBlockNumber>& client_blk_n) const {
  return client_blk_n ? *client_blk_n : lastBlockNumber();
}

state_api::EVMTransaction FinalChain::toEvmTransaction(const SharedTransaction& trx) {
  return state_api::EVMTransaction{
      trx->getSender(), trx->getGasPrice(), trx->getReceiver(), trx->getNonce(),
      trx->getValue(),  trx->getGas(),      trx->getData(),
  };
}

void FinalChain::appendEvmTransactions(std::vector<state_api::EVMTransaction>& evm_trxs,
                                       const SharedTransactions& trxs) {
  std::transform(trxs.cbegin(), trxs.cend(), std::back_inserter(evm_trxs),
                 [](const auto& trx) { return toEvmTransaction(trx); });
}

BlocksBlooms FinalChain::blockBlooms(const h256& chunk_id) const {
  if (auto raw = db_->lookup(chunk_id, DbStorage::Columns::final_chain_log_blooms_index); !raw.empty()) {
    return dev::RLP(raw).toArray<LogBloom, c_bloomIndexSize>();
  }
  return {};
}

h256 FinalChain::blockBloomsChunkId(EthBlockNumber level, EthBlockNumber index) { return h256(index * 0xff + level); }

std::vector<EthBlockNumber> FinalChain::withBlockBloom(const LogBloom& b, EthBlockNumber from, EthBlockNumber to,
                                                       EthBlockNumber level, EthBlockNumber index) const {
  std::vector<EthBlockNumber> ret;
  auto uCourse = int_pow(c_bloomIndexSize, level + 1);
  auto uFine = int_pow(c_bloomIndexSize, level);
  auto obegin = index == from / uCourse ? from / uFine % c_bloomIndexSize : 0;
  auto oend = index == to / uCourse ? (to / uFine) % c_bloomIndexSize + 1 : c_bloomIndexSize;
  auto bb = blockBlooms(blockBloomsChunkId(level, index));
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

}  // namespace taraxa::final_chain