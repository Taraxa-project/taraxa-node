#include "transaction/transaction_manager.hpp"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>

#include "common/constants.hpp"
#include "common/thread_pool.hpp"
#include "config/config.hpp"
#include "logger/logger.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
TransactionManager::TransactionManager(const FullNodeConfig &conf, std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<final_chain::FinalChain> final_chain, addr_t node_addr)
    : kConf(conf),
      transactions_pool_(final_chain, kConf.transactions_pool_size),
      estimations_cache_(kConf.transactions_pool_size / 10, kConf.transactions_pool_size / 100),
      kDagBlockGasLimit(kConf.genesis.dag.gas_limit),
      db_(std::move(db)),
      final_chain_(std::move(final_chain)),
      estimation_thread_pool_(std::thread::hardware_concurrency() / 2) {
  LOG_OBJECTS_CREATE("TRXMGR");
  {
    std::unique_lock transactions_lock(transactions_mutex_);
    trx_count_ = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  }
}

uint64_t TransactionManager::estimateTransactions(const SharedTransactions &trxs, PbftPeriod proposal_period) {
  std::atomic<uint64_t> total_gas = 0;
  std::vector<std::future<void>> futures;
  futures.reserve(trxs.size());
  for (const auto &trx : trxs) {
    if (trx->getGas() <= kEstimateGasLimit) {
      total_gas += trx->getGas();
    } else {
      futures.emplace_back(
          estimation_thread_pool_.post([&]() { total_gas += estimateTransactionGas(trx, proposal_period).gas_used; }));
    }
  }
  for (auto &future : futures) {
    future.get();
  }

  return total_gas.load();
}

state_api::ExecutionResult TransactionManager::estimateTransactionGas(std::shared_ptr<Transaction> trx,
                                                                      PbftPeriod proposal_period) {
  if (trx->getGas() <= kEstimateGasLimit) {
    state_api::ExecutionResult result;
    result.gas_used = trx->getGas();
    return result;
  }

  dev::RLPStream hash_rlp(2);
  hash_rlp << trx->getHash();
  hash_rlp << proposal_period;
  const auto hash = dev::sha3(hash_rlp.invalidate());

  if (const auto [cached_estimation, found] = estimations_cache_.get(hash); found) {
    return cached_estimation;
  }

  const auto isBeforeSoleiroliaHF = !kConf.genesis.state.hardforks.isOnSoleiroliaHardfork(proposal_period);

  auto evm_trx = state_api::EVMTransaction{
      trx->getSender(), trx->getGasPrice(), trx->getReceiver(), trx->getNonce(),
      trx->getValue(),  trx->getGas(),      trx->getData(),
  };

  if (isBeforeSoleiroliaHF) {
    evm_trx.gas = kDagBlockGasLimit;
  }

  auto result = final_chain_->call(evm_trx, proposal_period);

  if (!result.code_err.empty() || !result.consensus_err.empty()) {
    if (isBeforeSoleiroliaHF) {
      result.gas_used = 0;
    }
  }
  estimations_cache_.insert(hash, result);
  return result;
}

std::pair<bool, std::string> TransactionManager::verifyTransaction(const std::shared_ptr<Transaction> &trx,
                                                                   bool from_dag) const {
  // ONLY FOR TESTING
  if (!final_chain_) [[unlikely]] {
    return {true, ""};
  }

  if (trx->getChainID() != kConf.genesis.chain_id) {
    return {false,
            "chain_id mismatch " + std::to_string(trx->getChainID()) + " " + std::to_string(kConf.genesis.chain_id)};
  }

  const int64_t block_num = final_chain_->lastBlockNumber();
  const auto isOnSoleiroliaHF = kConf.genesis.state.hardforks.isOnSoleiroliaHardfork(
      std::max(block_num - (from_dag ? kDagExpiryLevelLimit : 0), int64_t(0)));

  // Ensure the transaction doesn't exceed the current block limit gas.
  if (isOnSoleiroliaHF) {
    if (kConf.genesis.state.hardforks.soleirolia_hf.trx_max_gas_limit < trx->getGas()) {
      return {false, "invalid gas"};
    }
  } else {
    if (kDagBlockGasLimit < trx->getGas()) {
      return {false, "invalid gas"};
    }
  }

  if (kConf.genesis.state.hardforks.isOnCornusHardfork(block_num)) {
    if (!trx->intrinsicGasCovered()) {
      return {false, "intrinsic gas too low"};
    }
  }

  try {
    trx->getSender();
  } catch (Transaction::InvalidSignature const &) {
    return {false, "invalid signature"};
  }

  // gas_price in transaction must be greater than or equal to minimum value from config
  if (isOnSoleiroliaHF) {
    if (kConf.genesis.state.hardforks.soleirolia_hf.trx_min_gas_price > trx->getGasPrice()) {
      return {false, "gas_price too low"};
    }
  } else {
    if (kConf.genesis.gas_price.minimum_price > trx->getGasPrice()) {
      return {false, "gas_price too low"};
    }
  }

  return {true, ""};
}

bool TransactionManager::isTransactionKnown(const trx_hash_t &trx_hash) {
  return transactions_pool_.isTransactionKnown(trx_hash);
}

std::pair<bool, std::string> TransactionManager::insertTransaction(const std::shared_ptr<Transaction> &trx) {
  if (isTransactionKnown(trx->getHash())) {
    return {false, "Transaction already in transactions pool"};
  }

  const auto [verified, reason] = verifyTransaction(trx);
  if (!verified) {
    return {false, reason};
  }

  const auto trx_hash = trx->getHash();
  auto trx_copy = trx;
  if (insertValidatedTransaction(std::move(trx_copy), false) == TransactionStatus::Inserted) {
    return {true, ""};
  } else {
    const auto location = db_->getTransactionLocation(trx_hash);
    if (location) {
      return {false, "Transaction already finalized in period" + std::to_string(location->period)};
    } else {
      return {false, "Transaction could not be inserted"};
    }
  }
}

TransactionStatus TransactionManager::insertValidatedTransaction(std::shared_ptr<Transaction> &&tx,
                                                                 bool insert_non_proposable) {
  const auto trx_hash = tx->getHash();

  // This lock synchronizes inserting and removing transactions from transactions memory pool.
  // It is very important to lock transaction pool checking to be
  // protected from new DAG block and Period data transactions insertions.
  std::unique_lock transactions_lock(transactions_mutex_);

  if (nonfinalized_transactions_in_dag_.contains(trx_hash)) {
    LOG(log_dg_) << "Transaction " << trx_hash << " in nonfinalized_transactions_in_dag_";
    return TransactionStatus::Known;
  }

  if (recently_finalized_transactions_.contains(trx_hash)) {
    LOG(log_dg_) << "Transaction " << trx_hash << " in recently_finalized_transactions_";
    return TransactionStatus::Known;
  }

  const auto account = final_chain_->getAccount(tx->getSender());
  bool proposable = true;

  if (account.has_value()) {
    // Ensure the transaction adheres to nonce ordering
    if (account->nonce > tx->getNonce()) {
      if (!insert_non_proposable) {
        LOG(log_dg_) << "Transaction " << trx_hash << " nonce: " << tx->getNonce()
                     << " smaller or equal to account nonce " << account->nonce;
        return TransactionStatus::Known;
      }
      proposable = false;
    }

    // Transactor should have enough funds to cover the costs
    // cost == V + GP * GL
    if (account->balance < tx->getCost()) {
      if (!insert_non_proposable) {
        LOG(log_dg_) << "Transaction " << trx_hash << " cost: " << tx->getCost()
                     << " smaller or equal to account balance " << account->balance;
        return TransactionStatus::Known;
      }
      proposable = false;
    }
  } else {
    if (!insert_non_proposable) {
      LOG(log_dg_) << "Transaction " << trx_hash << " account has no value ";
      return TransactionStatus::Known;
    }
    proposable = false;
  }

  if (kConf.propose_dag_gas_limit < tx->getGas()) {
    if (!insert_non_proposable) {
      LOG(log_dg_) << "Transaction " << trx_hash << " gas: " << tx->getGas() << " smaller or equal to gas limit "
                   << kConf.propose_dag_gas_limit;
      return TransactionStatus::Known;
    }
    proposable = false;
  }

  const auto last_block_number = final_chain_->lastBlockNumber();
  LOG(log_dg_) << "Transaction " << trx_hash << " inserted in trx pool";
  return transactions_pool_.insert(std::move(tx), proposable, last_block_number);
}

unsigned long TransactionManager::getTransactionCount() const {
  std::shared_lock shared_transactions_lock(transactions_mutex_);
  return trx_count_;
}

std::pair<std::vector<std::shared_ptr<Transaction>>, std::vector<trx_hash_t>> TransactionManager::getPoolTransactions(
    const std::vector<trx_hash_t> &trx_to_query) const {
  std::shared_lock transactions_lock(transactions_mutex_);
  std::pair<std::vector<std::shared_ptr<Transaction>>, std::vector<trx_hash_t>> result;
  for (const auto &hash : trx_to_query) {
    auto trx = transactions_pool_.get(hash);
    if (trx) {
      result.first.emplace_back(trx);
    } else {
      result.second.emplace_back(hash);
    }
  }
  return result;
}

std::shared_ptr<Transaction> TransactionManager::getTransaction(trx_hash_t const &hash) const {
  std::shared_lock transactions_lock(transactions_mutex_);
  auto trx = transactions_pool_.get(hash);
  if (trx) {
    return trx;
  }
  return db_->getTransaction(hash);
}

void TransactionManager::saveTransactionsFromDagBlock(SharedTransactions const &trxs) {
  std::vector<trx_hash_t> accepted_transactions;
  accepted_transactions.reserve(trxs.size());
  auto write_batch = db_->createWriteBatch();
  vec_trx_t trx_hashes;
  std::transform(trxs.begin(), trxs.end(), std::back_inserter(trx_hashes),
                 [](std::shared_ptr<Transaction> const &t) { return t->getHash(); });

  {
    // This lock synchronizes inserting and removing transactions from transactions memory pool with database
    // insertion. Unique lock here makes sure that transactions we are removing are not reinserted in
    // transactions_pool_
    std::unique_lock transactions_lock(transactions_mutex_);

    for (auto t : trxs) {
      const auto tx_hash = t->getHash();

      bool transaction_in_dag_or_finalized =
          nonfinalized_transactions_in_dag_.contains(tx_hash) || recently_finalized_transactions_.contains(tx_hash);
      if (transaction_in_dag_or_finalized) {
        continue;
      }

      // Checking nonce in cheaper than checking db, verify with nonce if possible
      const auto account = final_chain_->getAccount(t->getSender()).value_or(taraxa::state_api::ZeroAccount);
      if (account.nonce >= t->getNonce()) {
        // This is a very rare scenario but it can happen:
        // The check against database is needed because there is a possibility that transaction was executed within last
        // 100 period (dag proposal period) but it might not be part of recently_finalized_transactions_
        transaction_in_dag_or_finalized = db_->transactionFinalized(tx_hash);
      }

      if (!transaction_in_dag_or_finalized) {
        db_->addTransactionToBatch(*t, write_batch);
        nonfinalized_transactions_in_dag_.emplace(tx_hash, t);
        if (transactions_pool_.erase(t)) {
          LOG(log_dg_) << "Transaction " << tx_hash << " removed from trx pool ";
          // Transactions are counted when included in DAG
          accepted_transactions.emplace_back(tx_hash);
        }
        trx_count_++;
      }
    }
    db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count_, write_batch);
    db_->commitWriteBatch(write_batch);
  }
  for (const auto &trx_hash : accepted_transactions) {
    transaction_accepted_.emit(trx_hash);
  }
}

void TransactionManager::recoverNonfinalizedTransactions() {
  std::unique_lock transactions_lock(transactions_mutex_);
  // On restart populate nonfinalized_transactions_in_dag_ structure from db
  auto trxs = db_->getAllNonfinalizedTransactions();
  vec_trx_t trx_hashes;
  trx_hashes.reserve(trxs.size());
  std::transform(trxs.begin(), trxs.end(), std::back_inserter(trx_hashes),
                 [](std::shared_ptr<Transaction> const &t) { return t->getHash(); });
  auto trx_finalized = db_->transactionsFinalized(trx_hashes);
  auto write_batch = db_->createWriteBatch();
  for (uint64_t i = 0; i < trxs.size(); i++) {
    auto const &trx_hash = trx_hashes[i];
    if (trx_finalized[i]) {
      // TODO: This section where transactions are deleted is only to recover from a bug where non-finalized
      // transactions were not properly deleted from the DB, once there are no nodes with such corrupted data, this
      // line can be removed or replaced with an assert
      db_->removeTransactionToBatch(trx_hash, write_batch);
    } else {
      // Cache sender now by calling getSender since getting sender later on proposing blocks can affect performance
      trxs[i]->getSender();
      nonfinalized_transactions_in_dag_.emplace(trx_hash, std::move(trxs[i]));
    }
  }
  db_->commitWriteBatch(write_batch);
}

size_t TransactionManager::getTransactionPoolSize() const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.size();
}

bool TransactionManager::nonProposableTransactionsOverTheLimit() const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.nonProposableTransactionsOverTheLimit();
}

bool TransactionManager::isTransactionPoolFull(size_t percentage) const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.size() >= (kConf.transactions_pool_size * percentage / 100);
}

size_t TransactionManager::getNonfinalizedTrxSize() const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return nonfinalized_transactions_in_dag_.size();
}

std::vector<std::shared_ptr<Transaction>> TransactionManager::getNonfinalizedTrx(
    const std::vector<trx_hash_t> &hashes) {
  std::vector<std::shared_ptr<Transaction>> ret;
  ret.reserve(hashes.size());
  std::shared_lock transactions_lock(transactions_mutex_);
  for (const auto &hash : hashes) {
    if (nonfinalized_transactions_in_dag_.contains(hash)) {
      ret.push_back(nonfinalized_transactions_in_dag_[hash]);
    }
  }
  return ret;
}

std::unordered_set<trx_hash_t> TransactionManager::excludeFinalizedTransactions(const std::vector<trx_hash_t> &hashes) {
  std::unordered_set<trx_hash_t> ret;
  ret.reserve(hashes.size());
  std::shared_lock transactions_lock(transactions_mutex_);
  for (const auto &hash : hashes) {
    if (!recently_finalized_transactions_.contains(hash)) {
      if (!db_->transactionFinalized(hash)) {
        ret.insert(hash);
      }
    }
  }
  return ret;
}

bool TransactionManager::verifyTransactionsNotFinalized(const SharedTransactions &trxs) {
  for (auto t : trxs) {
    const auto tx_hash = t->getHash();

    if (recently_finalized_transactions_.contains(tx_hash)) {
      LOG(log_er_) << "Transaction " << tx_hash << " already finalized";
      return false;
    }

    // Checking nonce in cheaper than checking db, verify with nonce if possible
    const auto account = final_chain_->getAccount(t->getSender()).value_or(taraxa::state_api::ZeroAccount);
    if (account.nonce >= t->getNonce()) {
      // This is a very rare scenario but it can happen:
      // The check against database is needed because there is a possibility that transaction was executed within last
      // 100 period (dag proposal period) but it might not be part of recently_finalized_transactions_
      if (db_->transactionFinalized(tx_hash)) {
        LOG(log_er_) << "Transaction " << tx_hash << " already finalized in db";
        return false;
      }
    }
  }
  return true;
}

/**
 * Retrieve transactions to be included in proposed block
 */
std::pair<SharedTransactions, std::vector<uint64_t>> TransactionManager::packTrxs(PbftPeriod proposal_period,
                                                                                  uint64_t weight_limit) {
  SharedTransactions trxs;
  const uint64_t max_transactions_in_block = weight_limit / kMinTxGas;
  const auto isBeforeSoleiroliaHF = !kConf.genesis.state.hardforks.isOnSoleiroliaHardfork(proposal_period);

  // Remove after Soleirolia hardfork
  const uint32_t max_zero_gas_transactions_per_block = 2;
  uint32_t zero_gas_transactions_count = 0;

  {
    std::shared_lock transactions_lock(transactions_mutex_);
    trxs = transactions_pool_.getOrderedTransactions(max_transactions_in_block);
  }

  std::vector<uint64_t> estimations;
  SharedTransactions trxs_to_propose;
  uint64_t total_weight = 0;
  for (uint64_t i = 0; i < trxs.size(); i++) {
    // trx too big to fit, skip it
    if (total_weight + trxs[i]->getGas() > weight_limit) {
      continue;
    }

    auto estimate = estimateTransactionGas(trxs[i], proposal_period);
    if (isBeforeSoleiroliaHF) {
      // Estimate 0 possible only before Soleirolia hardfork, remove after
      if (estimate.gas_used < kMinTxGas) {
        if (estimate.code_err == "out of gas") {
          std::unique_lock transactions_lock(transactions_mutex_);
          auto trx = trxs[i];
          transactions_pool_.erase(trx);
          transactions_pool_.insert(std::move(trx), false, final_chain_->lastBlockNumber());
          continue;
        } else {
          if (zero_gas_transactions_count > max_zero_gas_transactions_per_block) {
            continue;
          }
          zero_gas_transactions_count++;
        }
      }
    } else {
      if (estimate.gas_used < kMinTxGas) {
        LOG(log_er_) << "Transaction " << trxs[i]->getHash() << " has invalid estimation: " << estimate.gas_used;
        std::unique_lock transactions_lock(transactions_mutex_);
        auto trx = trxs[i];
        transactions_pool_.erase(trx);
        transactions_pool_.insert(std::move(trx), false, final_chain_->lastBlockNumber());
        continue;
      }
    }

    total_weight += estimate.gas_used;
    trxs_to_propose.push_back(trxs[i]);
    estimations.push_back(estimate.gas_used);
    // stop if there is no space for even the smallest transaction
    if (weight_limit - total_weight <= kMinTxGas) {
      break;
    }
  }
  return {trxs_to_propose, estimations};
}

std::vector<SharedTransactions> TransactionManager::getAllPoolTrxs() {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.getAllTransactions();
}

void TransactionManager::initializeRecentlyFinalizedTransactions(const PeriodData &period_data) {
  std::unique_lock transactions_lock(transactions_mutex_);
  for (auto const &trx : period_data.transactions) {
    const auto hash = trx->getHash();
    recently_finalized_transactions_[hash] = trx;
    recently_finalized_transactions_per_period_[period_data.pbft_blk->getPeriod()].push_back(hash);
  }
}

/**
 * Update transaction counters and state, it only has effect when PBFT syncing
 */
void TransactionManager::updateFinalizedTransactionsStatus(PeriodData const &period_data) {
  // !!! There is no lock because it is called under std::unique_lock trx_lock(trx_mgr_->getTransactionsMutex());
  const auto recently_finalized_transactions_periods =
      kRecentlyFinalizedTransactionsFactor * final_chain_->delegationDelay();
  // Delete transactions older than recently_finalized_transactions_periods
  if (period_data.pbft_blk->getPeriod() > recently_finalized_transactions_periods) {
    for (auto hash : recently_finalized_transactions_per_period_[period_data.pbft_blk->getPeriod() -
                                                                 recently_finalized_transactions_periods]) {
      recently_finalized_transactions_.erase(hash);
    }
    recently_finalized_transactions_per_period_.erase(period_data.pbft_blk->getPeriod() -
                                                      recently_finalized_transactions_periods);
  }
  if (period_data.transactions.size() > 0) {
    for (auto const &trx : period_data.transactions) {
      const auto hash = trx->getHash();
      recently_finalized_transactions_[hash] = trx;
      recently_finalized_transactions_per_period_[period_data.pbft_blk->getPeriod()].push_back(hash);
      // we should mark trx as know just in case it was only synchronized from the period data
      transactions_pool_.markTransactionKnown(hash);
      if (!nonfinalized_transactions_in_dag_.erase(hash)) {
        trx_count_++;
      } else {
        LOG(log_dg_) << "Transaction " << hash << " removed from nonfinalized transactions";
      }
      if (transactions_pool_.erase(trx)) {
        LOG(log_dg_) << "Transaction " << hash << " removed from transactions_pool_";
      }
    }
    db_->saveStatusField(StatusDbField::TrxCount, trx_count_);
  }

  // Sometimes transactions which nonce was skipped and will never be included in a block might remain in the
  // transaction pool, remove them each 100 block
  const uint32_t transaction_purge_interval = 100;
  if (period_data.pbft_blk->getPeriod() % transaction_purge_interval == 0) {
    transactions_pool_.purge();
  }
}

void TransactionManager::removeNonFinalizedTransactions(std::unordered_set<trx_hash_t> &&transactions) {
  // !!! There is no lock because it is called under std::unique_lock trx_lock(trx_mgr_->getTransactionsMutex());
  auto write_batch = db_->createWriteBatch();
  for (auto const &trx_hash : transactions) {
    auto trx = nonfinalized_transactions_in_dag_.find(trx_hash);
    if (trx != nonfinalized_transactions_in_dag_.end()) {
      db_->removeTransactionToBatch(trx_hash, write_batch);
      nonfinalized_transactions_in_dag_.erase(trx);
    }
  }
  db_->commitWriteBatch(write_batch);
}

SharedTransactions TransactionManager::getBlockTransactions(const DagBlock &blk, PbftPeriod proposal_period) {
  return getTransactions(blk.getTrxs(), proposal_period);
}

SharedTransactions TransactionManager::getTransactions(const vec_trx_t &trxs_hashes, PbftPeriod proposal_period) {
  vec_trx_t finalized_trx_hashes;
  SharedTransactions transactions;
  transactions.reserve(trxs_hashes.size());
  for (auto const &tx_hash : trxs_hashes) {
    std::shared_lock transactions_lock(transactions_mutex_);
    auto trx = transactions_pool_.get(tx_hash);
    if (trx != nullptr) {
      transactions.emplace_back(std::move(trx));
    } else {
      auto trx_it = nonfinalized_transactions_in_dag_.find(tx_hash);
      if (trx_it != nonfinalized_transactions_in_dag_.end()) {
        transactions.emplace_back(trx_it->second);
      } else {
        trx_it = recently_finalized_transactions_.find(tx_hash);
        if (trx_it != recently_finalized_transactions_.end()) {
          transactions.emplace_back(trx_it->second);
        } else {
          finalized_trx_hashes.emplace_back(tx_hash);
        }
      }
    }
  }

  // This should be an extremely rare case since transactions should be found in the caches
  auto finalizedTransactions = db_->getFinalizedTransactions(finalized_trx_hashes);

  for (auto trx : finalizedTransactions) {
    // Only include transactions with valid nonce at proposal period
    auto acc = final_chain_->getAccount(trx->getSender(), proposal_period);
    if (acc.has_value() && acc->nonce > trx->getNonce()) {
      LOG(log_er_) << "Old transaction: " << trx->getHash();
    } else {
      transactions.emplace_back(std::move(trx));
    }
  }
  return transactions;
}

void TransactionManager::blockFinalized(EthBlockNumber block_number) {
  std::unique_lock transactions_lock(transactions_mutex_);
  transactions_pool_.blockFinalized(block_number);
}

bool TransactionManager::transactionsDropped() const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.transactionsDropped();
}

}  // namespace taraxa
