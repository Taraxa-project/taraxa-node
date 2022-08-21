#include "transaction/transaction_manager.hpp"

#include <string>
#include <utility>

#include "dag/dag.hpp"
#include "logger/logger.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
TransactionManager::TransactionManager(FullNodeConfig const &conf, std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<FinalChain> final_chain, addr_t node_addr)
    : kConf(conf),
      transactions_pool_(kConf.transactions_pool_size),
      db_(std::move(db)),
      final_chain_(std::move(final_chain)) {
  LOG_OBJECTS_CREATE("TRXMGR");
  {
    std::unique_lock transactions_lock(transactions_mutex_);
    trx_count_ = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  }
}

uint64_t TransactionManager::estimateTransactionGas(std::shared_ptr<Transaction> trx,
                                                    std::optional<uint64_t> proposal_period) const {
  const auto &result = final_chain_->call(
      state_api::EVMTransaction{
          trx->getSender(),
          trx->getGasPrice(),
          trx->getReceiver(),
          trx->getNonce(),
          trx->getValue(),
          FinalChain::GAS_LIMIT,
          trx->getData(),
      },
      proposal_period);

  if (!result.code_err.empty() || !result.consensus_err.empty()) {
    return 0;
  }
  return result.gas_used;
}

std::pair<TransactionStatus, std::string> TransactionManager::verifyTransaction(
    const std::shared_ptr<Transaction> &trx) const {
  // ONLY FOR TESTING
  if (!final_chain_) [[unlikely]] {
    return {TransactionStatus::Verified, ""};
  }

  if (trx->getChainID() != kConf.chain_id) {
    return {TransactionStatus::Invalid,
            "chain_id mismatch " + std::to_string(trx->getChainID()) + " " + std::to_string(kConf.chain_id)};
  }

  // Ensure the transaction doesn't exceed the current block limit gas.
  if (FinalChain::GAS_LIMIT < trx->getGas()) {
    return {TransactionStatus::Invalid, "invalid gas"};
  }

  try {
    trx->getSender();
  } catch (Transaction::InvalidSignature const &) {
    return {TransactionStatus::Invalid, "invalid signature"};
  }

  const auto account = final_chain_->get_account(trx->getSender()).value_or(taraxa::state_api::ZeroAccount);

  // Ensure the transaction adheres to nonce ordering
  if (account.nonce && account.nonce > trx->getNonce()) {
    return {TransactionStatus::LowNonce, "nonce too low"};
  }

  // Transactor should have enough funds to cover the costs
  // cost == V + GP * GL
  if (account.balance < trx->getCost()) {
    return {TransactionStatus::InsufficentBalance, "insufficient balance"};
  }

  return {TransactionStatus::Verified, ""};
}

bool TransactionManager::isTransactionKnown(const trx_hash_t &trx_hash) {
  return transactions_pool_.isTransactionKnown(trx_hash);
}

std::pair<bool, std::string> TransactionManager::insertTransaction(const std::shared_ptr<Transaction> &trx) {
  if (isTransactionKnown(trx->getHash())) {
    return {false, "Transaction already in transactions pool"};
  }

  const auto [status, reason] = verifyTransaction(trx);
  if (status != TransactionStatus::Verified) {
    return {false, reason};
  }

  if (insertValidatedTransactions({{trx, status}})) {
    return {true, "Can not insert transactions"};
  } else {
    const auto period = db_->getTransactionPeriod(trx->getHash());
    if (period != std::nullopt) {
      return {false, "Transaction already finalized in period" + std::to_string(period->first)};
    } else {
      return {false, "Transaction could not be inserted"};
    }
  }
}

uint32_t TransactionManager::insertValidatedTransactions(
    std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> &&txs) {
  std::vector<std::pair<std::shared_ptr<Transaction>, TransactionStatus>> unseen_txs;
  std::vector<trx_hash_t> txs_hashes;

  if (txs.empty()) {
    return 0;
  }

  txs_hashes.reserve(txs.size());
  std::transform(txs.begin(), txs.end(), std::back_inserter(txs_hashes),
                 [](const auto &t) { return t.first->getHash(); });

  // This lock synchronizes inserting and removing transactions from transactions memory pool with database insertion.
  // It is very important to lock checking the db state of transaction together with transaction pool checking to be
  // protected from new DAG block and Period data transactions insertions which are inserted directly in the database.
  std::unique_lock transactions_lock(transactions_mutex_);
  const auto last_block_number = final_chain_->last_block_number();

  // Check the db with a multiquery if transactions are really new
  auto db_seen = db_->transactionsInDb(txs_hashes);
  for (uint32_t i = 0; i < txs_hashes.size(); i++) {
    if (!db_seen[i]) {
      unseen_txs.push_back(std::move(txs[i]));
    } else {
      // In case we received a new tx that is already in db, mark it as known in cache
      transactions_pool_.markTransactionKnown(txs[i].first->getHash());
    }
  }

  size_t trx_inserted_count = 0;
  // Save transactions in memory pool
  for (auto &trx : unseen_txs) {
    auto tx_hash = trx.first->getHash();
    LOG(log_dg_) << "Transaction " << tx_hash << " inserted in trx pool";
    if (transactions_pool_.insert(std::move(trx), last_block_number)) {
      trx_inserted_count++;
    }
  }
  return trx_inserted_count;
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
    // This lock synchronizes inserting and removing transactions from transactions memory pool with database insertion.
    // Unique lock here makes sure that transactions we are removing are not reinserted in transactions_pool_
    std::unique_lock transactions_lock(transactions_mutex_);

    auto trx_in_db = db_->transactionsInDb(trx_hashes);
    for (uint64_t i = 0; i < trxs.size(); i++) {
      auto const &trx_hash = trx_hashes[i];
      // We only save transaction if it has not already been saved
      if (!trx_in_db[i]) {
        db_->addTransactionToBatch(*trxs[i], write_batch);
        nonfinalized_transactions_in_dag_.emplace(trx_hash, trxs[i]);
      }
      if (transactions_pool_.erase(trx_hash)) {
        LOG(log_dg_) << "Transaction " << trx_hash << " removed from trx pool ";
        // Transactions are counted when included in DAG
        trx_count_++;
        accepted_transactions.emplace_back(trx_hash);
      }
    }
    db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count_, write_batch);
    db_->commitWriteBatch(write_batch);
  }
  for (auto &trx_hash : accepted_transactions) transaction_accepted_.emit(trx_hash);
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
      nonfinalized_transactions_in_dag_.emplace(trx_hash, std::move(trxs[i]));
    }
  }
  db_->commitWriteBatch(write_batch);
}

size_t TransactionManager::getTransactionPoolSize() const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.size();
}

bool TransactionManager::isTransactionPoolFull(size_t precentage) const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.size() >= (kConf.transactions_pool_size * precentage / 100);
}

size_t TransactionManager::getNonfinalizedTrxSize() const {
  std::shared_lock transactions_lock(transactions_mutex_);
  return nonfinalized_transactions_in_dag_.size();
}

std::vector<std::shared_ptr<Transaction>> TransactionManager::getNonfinalizedTrx(const std::vector<trx_hash_t> &hashes,
                                                                                 bool sorted) {
  std::vector<std::shared_ptr<Transaction>> ret;
  ret.reserve(hashes.size());
  std::shared_lock transactions_lock(transactions_mutex_);
  for (const auto &hash : hashes) {
    if (nonfinalized_transactions_in_dag_.contains(hash)) {
      ret.push_back(nonfinalized_transactions_in_dag_[hash]);
    }
  }
  if (sorted) {
    std::stable_sort(ret.begin(), ret.end(), [](const auto &t1, const auto &t2) {
      if (t1->getSender() == t2->getSender()) {
        return t1->getNonce() < t2->getNonce() ||
               (t1->getNonce() == t2->getNonce() && t1->getGasPrice() > t2->getGasPrice());
      }
      return true;
    });
  }
  return ret;
}

/**
 * Retrieve transactions to be included in proposed block
 */
std::pair<SharedTransactions, std::vector<uint64_t>> TransactionManager::packTrxs(uint64_t proposal_period,
                                                                                  uint64_t weight_limit) {
  SharedTransactions trxs;
  std::vector<uint64_t> estimations;
  uint64_t total_weight = 0;
  const uint64_t max_transactions_in_block = weight_limit / kMinTxGas;
  {
    std::shared_lock transactions_lock(transactions_mutex_);
    trxs = transactions_pool_.get(max_transactions_in_block);
  }
  for (uint64_t i = 0; i < trxs.size(); i++) {
    uint64_t weight = estimateTransactionGas(trxs[i], proposal_period);
    total_weight += weight;
    if (total_weight > weight_limit) {
      trxs.resize(i);
      break;
    }
    estimations.push_back(weight);
  }
  return {trxs, estimations};
}

SharedTransactions TransactionManager::getAllPoolTrxs() {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.get();
}

/**
 * Update transaction counters and state, it only has effect when PBFT syncing
 */
void TransactionManager::updateFinalizedTransactionsStatus(PeriodData const &period_data) {
  // !!! There is no lock because it is called under std::unique_lock trx_lock(trx_mgr_->getTransactionsMutex());
  last_finalized_block_transactions_.clear();
  for (auto const &trx : period_data.transactions) {
    last_finalized_block_transactions_[trx->getHash()] = trx;
    if (!nonfinalized_transactions_in_dag_.erase(trx->getHash())) {
      trx_count_++;
    } else {
      LOG(log_dg_) << "Transaction " << trx->getHash() << " removed from nonfinalized transactions";
    }
    if (transactions_pool_.erase(trx->getHash())) {
      LOG(log_dg_) << "Transaction " << trx->getHash() << " removed from transactions_pool_";
    }
  }
  db_->saveStatusField(StatusDbField::TrxCount, trx_count_);
}

void TransactionManager::moveNonFinalizedTransactionsToTransactionsPool(std::unordered_set<trx_hash_t> &&transactions) {
  // !!! There is no lock because it is called under std::unique_lock trx_lock(trx_mgr_->getTransactionsMutex());
  auto write_batch = db_->createWriteBatch();
  for (auto const &trx_hash : transactions) {
    auto trx = nonfinalized_transactions_in_dag_.find(trx_hash);
    if (trx != nonfinalized_transactions_in_dag_.end()) {
      db_->removeTransactionToBatch(trx_hash, write_batch);
      transactions_pool_.insert({std::move(trx->second), TransactionStatus::Verified});
      nonfinalized_transactions_in_dag_.erase(trx);
    }
  }
  db_->commitWriteBatch(write_batch);
}

// Verify all block transactions are present
std::optional<std::map<trx_hash_t, std::shared_ptr<Transaction>>> TransactionManager::getBlockTransactions(
    DagBlock const &blk) {
  vec_trx_t finalized_trx_hashes;
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    LOG(log_er_) << "Ignore block " << blk.getHash() << " since it has no transactions";
    return std::nullopt;
  }

  std::map<trx_hash_t, std::shared_ptr<Transaction>> transactions;
  {
    std::shared_lock transactions_lock(transactions_mutex_);
    for (auto const &tx_hash : all_block_trx_hashes) {
      auto trx = transactions_pool_.get(tx_hash);
      if (trx != nullptr) {
        transactions.emplace(tx_hash, std::move(trx));
      } else {
        auto trx_it = nonfinalized_transactions_in_dag_.find(tx_hash);
        if (trx_it != nonfinalized_transactions_in_dag_.end()) {
          transactions.emplace(tx_hash, trx_it->second);
        } else {
          trx_it = last_finalized_block_transactions_.find(tx_hash);
          if (trx_it != last_finalized_block_transactions_.end()) {
            transactions.emplace(tx_hash, trx_it->second);
          } else {
            finalized_trx_hashes.emplace_back(tx_hash);
          }
        }
      }
    }
    auto finalizedTransactions = db_->getFinalizedTransactions(finalized_trx_hashes);
    if (finalizedTransactions.first.has_value()) {
      for (auto trx : *finalizedTransactions.first) {
        transactions.emplace(trx->getHash(), trx);
      }
    } else {
      LOG(log_er_) << "Block " << blk.getHash() << " has missing transaction " << finalizedTransactions.second;
      return std::nullopt;
    }
  }

  return transactions;
}

void TransactionManager::blockFinalized(uint64_t block_number) {
  std::unique_lock transactions_lock(transactions_mutex_);
  transactions_pool_.blockFinalized(block_number);
}

}  // namespace taraxa
