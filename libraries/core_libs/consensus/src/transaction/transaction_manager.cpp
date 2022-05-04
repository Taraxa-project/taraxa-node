#include "transaction/transaction_manager.hpp"

#include <string>
#include <utility>

#include "dag/dag.hpp"
#include "logger/logger.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
TransactionManager::TransactionManager(FullNodeConfig const &conf, std::shared_ptr<DbStorage> db,
                                       std::shared_ptr<FinalChain> final_chain, addr_t node_addr)
    : conf_(conf),
      known_txs_(200000 /*capacity*/, 2000 /*delete step*/),
      db_(std::move(db)),
      final_chain_(std::move(final_chain)) {
  LOG_OBJECTS_CREATE("TRXMGR");
  {
    std::unique_lock transactions_lock(transactions_mutex_);
    trx_count_ = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  }
}

std::pair<bool, std::string> TransactionManager::verifyTransaction(const std::shared_ptr<Transaction> &trx) const {
  // ONLY FOR TESTING
  if (!final_chain_) [[unlikely]] {
    return {true, ""};
  }

  if (trx->getChainID() != conf_.chain.chain_id) {
    return {false, "chain_id mismatch"};
  }

  // Ensure the transaction doesn't exceed the current block limit gas.
  if (FinalChain::GAS_LIMIT < trx->getGas()) {
    return {false, "invalid gas"};
  }

  try {
    trx->getSender();
  } catch (Transaction::InvalidSignature const &) {
    return {false, "invalid signature"};
  }

  const auto account = final_chain_->get_account(trx->getSender()).value_or(taraxa::state_api::ZeroAccount);

  // Ensure the transaction adheres to nonce ordering
  if (account.nonce && account.nonce > trx->getNonce()) {
    return {false, "nonce too low"};
  }

  // Transactor should have enough funds to cover the costs
  // cost == V + GP * GL
  if (account.balance < trx->getCost()) {
    return {false, "insufficient balance"};
  }

  return {true, ""};
}

bool TransactionManager::checkMemoryPoolOverflow() {
  size_t queue_overflow_warn = conf_.test_params.max_transactions_pool_warn;
  size_t queue_overflow_drop = conf_.test_params.max_transactions_pool_drop;

  if (queue_overflow_drop && getTransactionPoolSize() >= queue_overflow_drop) {
    LOG(log_wr_) << "Transaction pool size: " << getTransactionPoolSize()
                 << " --> New transactions will not be processed !";
    return true;
  } else if (queue_overflow_warn && getTransactionPoolSize() >= queue_overflow_warn) {
    LOG(log_wr_) << "Transaction pool size: " << getTransactionPoolSize()
                 << " --> Only warning, transactions will bed processed.";
  }

  return false;
}

void TransactionManager::markTransactionKnown(const trx_hash_t &trx_hash) { known_txs_.insert(trx_hash); }

bool TransactionManager::isTransactionKnown(const trx_hash_t &trx_hash) { return known_txs_.contains(trx_hash); }

std::pair<bool, std::string> TransactionManager::insertTransaction(const Transaction &trx) {
  if (isTransactionKnown(trx.getHash())) {
    return {false, "Transaction already in transactions pool"};
  }

  {
    std::shared_lock transactions_lock(transactions_mutex_);
    if (transactions_pool_.contains(trx.getHash())) {
      return {false, "Transaction already in transactions pool"};
    } else if (nonfinalized_transactions_in_dag_.contains(trx.getHash())) {
      return {false, "Transaction already included in DAG block"};
    }
  }

  const auto trx_ptr = std::make_shared<Transaction>(trx);
  if (const auto [is_valid, reason] = verifyTransaction(trx_ptr); !is_valid) {
    return {false, reason};
  }

  if (insertValidatedTransactions({trx_ptr})) {
    return {true, "Can not insert transactions"};
  } else {
    const auto period = db_->getTransactionPeriod(trx.getHash());
    if (period != std::nullopt) {
      return {false, "Transaction already finalized in period" + std::to_string(period->first)};
    } else {
      return {false, "Transaction could not be inserted"};
    }
  }
}

uint32_t TransactionManager::insertValidatedTransactions(SharedTransactions &&txs) {
  SharedTransactions unseen_txs;
  std::vector<trx_hash_t> txs_hashes;

  if (txs.empty()) {
    return 0;
  }

  if (checkMemoryPoolOverflow()) {
    LOG(log_er_) << "Pool overflow";
    return 0;
  }
  txs_hashes.reserve(txs.size());
  std::transform(txs.begin(), txs.end(), std::back_inserter(txs_hashes), [](const auto &t) { return t->getHash(); });

  // This lock synchronizes inserting and removing transactions from transactions memory pool with database insertion.
  // It is very important to lock checking the db state of transaction together with transaction pool checking to be
  // protected from new DAG block and Sync block transactions insertions which are inserted directly in the database.
  std::unique_lock transactions_lock(transactions_mutex_);

  // Check the db with a multiquery if transactions are really new
  auto db_seen = db_->transactionsInDb(txs_hashes);
  for (uint32_t i = 0; i < txs_hashes.size(); i++) {
    if (!db_seen[i]) {
      unseen_txs.push_back(std::move(txs[i]));
    } else {
      // In case we received a new tx that is already in db, mark it as known in cache
      markTransactionKnown(txs[i]->getHash());
    }
  }

  size_t trx_inserted_count = 0;
  // Save transactions in memory pool
  for (auto &trx : unseen_txs) {
    auto tx_hash = trx->getHash();
    LOG(log_dg_) << "Transaction " << tx_hash << " inserted in trx pool";
    if (transactions_pool_.insert(std::move(trx))) {
      trx_inserted_count++;
      markTransactionKnown(tx_hash);
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
  // This lock synchronizes inserting and removing transactions from transactions memory pool with database insertion.
  // Unique lock here makes sure that transactions we are removing are not reinserted in transactions_pool_
  std::unique_lock transactions_lock(transactions_mutex_);
  auto write_batch = db_->createWriteBatch();
  vec_trx_t trx_hashes;
  std::transform(trxs.begin(), trxs.end(), std::back_inserter(trx_hashes),
                 [](std::shared_ptr<Transaction> const &t) { return t->getHash(); });
  auto trx_in_db = db_->transactionsInDb(trx_hashes);
  for (uint64_t i = 0; i < trxs.size(); i++) {
    auto const &trx_hash = trx_hashes[i];
    // We only save transaction if it has not already been saved
    if (!trx_in_db[i]) {
      db_->addTransactionToBatch(*trxs[i], write_batch);
      nonfinalized_transactions_in_dag_.emplace(trx_hash, trxs[i]);
    }
    if (transactions_pool_.erase(trx_hash)) {
      LOG(log_dg_) << "Transaction " << trx_hash << " removed from trx pool";
      // Transactions are counted when included in DAG
      trx_count_++;
      transaction_accepted_.emit(trx_hash);
    }
  }
  db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count_, write_batch);
  db_->commitWriteBatch(write_batch);
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
      return t1->getGasPrice() > t2->getGasPrice();
    });
  }
  return ret;
}

/**
 * Retrieve transactions to be included in proposed block
 */
SharedTransactions TransactionManager::packTrxs(uint16_t max_trx_to_pack) {
  std::shared_lock transactions_lock(transactions_mutex_);
  return transactions_pool_.get(max_trx_to_pack);
}

/**
 * Update transaction counters and state, it only has effect when PBFT syncing
 */
void TransactionManager::updateFinalizedTransactionsStatus(SyncBlock const &sync_block) {
  // !!! There is no lock because it is called under std::unique_lock trx_lock(trx_mgr_->getTransactionsMutex());
  for (auto const &trx : sync_block.transactions) {
    if (!nonfinalized_transactions_in_dag_.erase(trx.getHash())) {
      trx_count_++;
    } else {
      LOG(log_dg_) << "Transaction " << trx.getHash() << " removed from nonfinalized transactions";
    }
    if (transactions_pool_.erase(trx.getHash())) {
      LOG(log_dg_) << "Transaction " << trx.getHash() << " removed from transactions_pool_";
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
      nonfinalized_transactions_in_dag_.erase(trx_hash);
      auto transaction = trx->second;
      transactions_pool_.insert(std::move(transaction));
    }
  }
  db_->commitWriteBatch(write_batch);
}

// Verify all block transactions are present
bool TransactionManager::checkBlockTransactions(DagBlock const &blk) {
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    LOG(log_er_) << "Ignore block " << blk.getHash() << " since it has no transactions";
    return false;
  }
  {
    std::shared_lock transactions_lock(transactions_mutex_);
    for (auto const &tx_hash : blk.getTrxs()) {
      if (!transactions_pool_.contains(tx_hash) && !nonfinalized_transactions_in_dag_.contains(tx_hash) &&
          !db_->transactionFinalized(tx_hash)) {
        LOG(log_er_) << "Block " << blk.getHash() << " has missing transaction " << tx_hash;
        return false;
      }
    }
  }

  return true;
}

}  // namespace taraxa
