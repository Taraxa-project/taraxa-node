#include "transaction_manager/transaction_manager.hpp"

#include <string>
#include <utility>

#include "dag/dag.hpp"
#include "logger/logger.hpp"
#include "transaction/transaction.hpp"

namespace taraxa {
auto trxComp = [](std::shared_ptr<Transaction> const &t1, std::shared_ptr<Transaction> const &t2) -> bool {
  if (t1->getSender() < t2->getSender())
    return true;
  else if (t1->getSender() == t2->getSender()) {
    return t1->getNonce() < t2->getNonce();
  } else {
    return false;
  }
};

TransactionManager::TransactionManager(FullNodeConfig const &conf, addr_t node_addr, std::shared_ptr<DbStorage> db,
                                       VerifyMode mode)
    : mode_(mode), conf_(conf), seen_txs_(200000 /*capacity*/, 2000 /*delete step*/), db_(db) {
  LOG_OBJECTS_CREATE("TRXMGR");
  trx_count_ = db_->getStatusField(taraxa::StatusDbField::TrxCount);
}

std::pair<bool, std::string> TransactionManager::verifyTransaction(Transaction const &trx) const {
  if (trx.getChainID() != conf_.chain.chain_id) {
    return {false, "chain_id mismatch"};
  }
  try {
    trx.getSender();
  } catch (Transaction::InvalidSignature const &) {
    return {false, "invalid signature"};
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

std::pair<bool, std::string> TransactionManager::insertTransaction(const Transaction &trx) {
  auto inserted = insertBroadcastedTransactions({std::make_shared<Transaction>(trx)});
  if (inserted) {
    return {true, ""};
  } else if (transactions_pool_.count(trx.getHash())) {
    return {false, "Transaction already in transactions pool"};
  } else if (nonfinalized_transactions_in_dag_.count(trx.getHash())) {
    return {false, "Transaction already included in DAG block"};
  } else {
    auto period = db_->getTransactionPeriod(trx.getHash());
    if (period != std::nullopt) {
      return {false, "Transaction already finalized in period" + std::to_string(period->first)};
    } else {
      return {false, "Transaction could not be inserted"};
    }
  }
}

uint32_t TransactionManager::insertBroadcastedTransactions(const SharedTransactions &txs) {
  SharedTransactions verified_txs;
  SharedTransactions unseen_txs;
  std::vector<trx_hash_t> verified_txs_hashes;

  if (checkMemoryPoolOverflow() == true) {
    LOG(log_er_) << "Pool overflow";
    return 0;
  }

  for (auto const &trx : txs) {
    // Ignore transactions that are already seen
    if (seen_txs_.count(trx->getHash()) != 0) {
      continue;
    }

    // Skip mode only used for testing
    if (mode_ != VerifyMode::skip_verify_sig) {
      if (const auto verified = verifyTransaction(*trx); verified.first == false) {
        // It is invalid so don't process again
        seen_txs_.insert(trx->getHash());
        LOG(log_er_) << "Trying to insert invalid trx, hash: " << trx->getHash() << ", err msg: " << verified.second;
        continue;
      }
    }

    verified_txs_hashes.emplace_back(trx->getHash());
    verified_txs.emplace_back(std::move(trx));
  }
  if (verified_txs.empty()) {
    return 0;
  }

  // This lock synchronizes inserting and removing transactions from transactions memory pool with database insertion.
  // It is very important to lock checking the db state of transaction together with transaction pool checking to be
  // protected from new DAG block and Sync block transactions insertions which are inserted directly in the database.
  std::shared_lock shared_transactions_lock(transactions_mutex_);

  // Check the db with a multiquery if transactions are really new
  auto db_seen = db_->transactionsInDb(verified_txs_hashes);
  for (uint32_t i = 0; i < verified_txs_hashes.size(); i++) {
    if (!db_seen[i]) {
      unseen_txs.emplace_back(std::move(verified_txs[i]));
    }
  }

  size_t trx_inserted_count = 0;
  // Save transactions in memory pool
  for (auto const &trx : unseen_txs) {
    LOG(log_dg_) << "Transaction " << trx->getHash() << " inserted in trx pool";
    if (transactions_pool_.emplace(trx->getHash(), (trx))) {
      trx_inserted_count++;
      seen_txs_.insert(trx->getHash());
      transactions_pool_changed_ = true;
    }
  }
  return trx_inserted_count;
}

unsigned long TransactionManager::getTransactionCount() const { return trx_count_; }

std::shared_ptr<Transaction> TransactionManager::getTransaction(trx_hash_t const &hash) const {
  std::shared_lock transactions_lock(transactions_mutex_);
  auto trx_it = transactions_pool_.get(hash);
  if (trx_it.second) {
    return trx_it.first;
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
      nonfinalized_transactions_in_dag_.emplace(trx_hash);
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
  auto trxs = db_->getNonfinalizedTransactions();
  vec_trx_t trx_hashes;
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
      nonfinalized_transactions_in_dag_.emplace(trx_hash);
    }
  }
  db_->commitWriteBatch(write_batch);
}

SharedTransactions TransactionManager::getTransactionsSnapShot() const { return transactions_pool_.getValues(); }

size_t TransactionManager::getTransactionPoolSize() const { return transactions_pool_.size(); }

size_t TransactionManager::getNonfinalizedTrxSize() const { return nonfinalized_transactions_in_dag_.size(); }

/**
 * Retrieve transactions to be included in proposed block
 */
SharedTransactions TransactionManager::packTrxs(uint16_t max_trx_to_pack) {
  SharedTransactions to_be_packed_trx = transactions_pool_.getValues(max_trx_to_pack);

  // sort trx based on sender and nonce
  std::sort(to_be_packed_trx.begin(), to_be_packed_trx.end(), trxComp);

  return to_be_packed_trx;
}

/**
 * Update transaction counters and state, it only has effect when PBFT syncing
 */
void TransactionManager::updateFinalizedTransactionsStatus(const vec_trx_t &txs) {
  // This lock synchronizes inserting and removing transactions from transactions memory pool with database insertion.
  // Unique lock here makes sure that transactions we are removing are not reinserted in transactions_pool_
  std::unique_lock transactions_lock(transactions_mutex_);
  for (auto const &tx_hash : txs) {
    if (!nonfinalized_transactions_in_dag_.erase(tx_hash)) {
      trx_count_++;
    } else {
      LOG(log_dg_) << "Transaction " << tx_hash << " removed from nonfinalized transactions";
    }
    if (transactions_pool_.erase(tx_hash)) {
      LOG(log_dg_) << "Transaction " << tx_hash << " removed from transactions_pool_";
    }
  }
  db_->saveStatusField(StatusDbField::TrxCount, trx_count_);
}

// Verify all block transactions are present
bool TransactionManager::checkBlockTransactions(DagBlock const &blk) {
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    LOG(log_er_) << "Ignore block " << blk.getHash() << " since it has no transactions";
    return false;
  }

  for (auto const &tx_hash : blk.getTrxs()) {
    if (transactions_pool_.count(tx_hash) == 0 && nonfinalized_transactions_in_dag_.count(tx_hash) == 0 &&
        !db_->transactionFinalized(tx_hash)) {
      LOG(log_er_) << "Block " << blk.getHash() << " has missing transaction " << tx_hash;
      return false;
    }
  }

  return true;
}

}  // namespace taraxa
