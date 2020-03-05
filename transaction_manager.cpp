#include "transaction_manager.hpp"
#include "transaction.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <utility>

#include "eth/util.hpp"
#include "full_node.hpp"

namespace taraxa {
auto trxComp = [](Transaction const &t1, Transaction const &t2) -> bool {
  if (t1.getSender() < t2.getSender())
    return true;
  else if (t1.getSender() == t2.getSender()) {
    return t1.getNonce() < t2.getNonce();
  } else {
    return false;
  }
};

bool TransactionManager::verifyTransaction(Transaction const &trx) const {
  try {
    if (eth_service_) {
      eth_service_->sealEngine()->verifyTransaction(
          dev::eth::ImportRequirements::Everything,
          eth::util::trx_taraxa_2_eth(trx),  //
          eth_service_->head(),    //
          0);
    } else {
      return trx.verifySig();
    }
    return true;
  } catch (...) {  // Once we are sure what exception can be thrown we should
                   // only catch those exceptions
  }
  return false;
}

void TransactionManager::verifyQueuedTrxs() {
  while (!stopped_) {
    // It will wait if no transaction in unverified queue
    auto item = trx_qu_.getUnverifiedTransaction();
    if (stopped_) return;

    bool valid = false;
    trx_hash_t hash = item.second->getHash();
    // verify and put the transaction to verified queue
    if (mode_ == VerifyMode::skip_verify_sig) {
      valid = true;
    } else {
      valid = verifyTransaction(*item.second);
    }
    // mark invalid
    if (!valid) {
      {
        uLock lock(mu_for_transactions_);
        db_->saveTransactionStatus(hash, TransactionStatus::invalid);
      }
      trx_qu_.removeTransactionFromBuffer(hash);
      LOG(log_wr_) << getFullNodeAddress() << " Trx: " << hash << "invalid. "
                   << std::endl;
      continue;
    }
    {
      uLock lock(mu_for_transactions_);
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_unverified) {
        db_->saveTransactionStatus(hash, TransactionStatus::in_queue_verified);
        lock.unlock();
        trx_qu_.addTransactionToVerifiedQueue(hash, item.second);
      }
    }
  }
}

void TransactionManager::setFullNode(std::shared_ptr<FullNode> full_node) {
  full_node_ = full_node;
  db_ = full_node->getDB();
  trx_qu_.setFullNode(full_node);
  auto trx_count = db_->getStatusField(taraxa::StatusDbField::TrxCount);
  trx_count_.store(trx_count);
  DagBlock blk;
  string pivot;
  std::vector<std::string> tips;
  full_node->getLatestPivotAndTips(pivot, tips);
  DagFrontier frontier;
  frontier.pivot = blk_hash_t(pivot);
  updateNonce(blk, frontier);
}

void TransactionManager::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  trx_qu_.start();
  verifiers_.clear();
  for (auto i = 0; i < num_verifiers_; ++i) {
    LOG(log_nf_) << "Create Transaction verifier ... " << std::endl;
    verifiers_.emplace_back([this]() { verifyQueuedTrxs(); });
  }
  assert(num_verifiers_ == verifiers_.size());
}

void TransactionManager::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return;
  }
  trx_qu_.stop();
  for (auto &t : verifiers_) {
    t.join();
  }
}

std::unordered_map<trx_hash_t, Transaction>
TransactionManager::getVerifiedTrxSnapShot() {
  return trx_qu_.getVerifiedTrxSnapShot();
}

std::pair<size_t, size_t> TransactionManager::getTransactionQueueSize() const {
  return trx_qu_.getTransactionQueueSize();
}

std::vector<taraxa::bytes>
TransactionManager::getNewVerifiedTrxSnapShotSerialized() {
  auto verified_trxs = trx_qu_.getNewVerifiedTrxSnapShot();
  std::vector<Transaction> vec_trxs;
  for (auto const &t : verified_trxs) vec_trxs.emplace_back(t);
  sort(vec_trxs.begin(), vec_trxs.end(), trxComp);
  std::vector<taraxa::bytes> ret;
  for (auto const &t : vec_trxs) {
    auto [trx_rlp, exist] = rlp_cache_.get(t.getHash());
    // if cached miss, compute on the fly
    ret.emplace_back(exist ? trx_rlp : t.rlp(true));
  }
  return ret;
}

unsigned long TransactionManager::getTransactionCount() const {
  return trx_count_.load();
}

std::shared_ptr<std::pair<Transaction, taraxa::bytes>>
TransactionManager::getTransaction(trx_hash_t const &hash) const {
  // Check the status
  std::shared_ptr<std::pair<Transaction, taraxa::bytes>> tr;
  // Loop needed because moving transactions from queue to database is not
  // secure Probably a better fix is to have transactions saved to the
  // database first and only then removed from the queue
  uint counter = 0;
  while (tr == nullptr) {
    auto t = trx_qu_.getTransaction(hash);
    if (t) {  // find in queue
      auto [trx_rlp, exist] = rlp_cache_.get(t->getHash());
      if (!exist) {
        LOG(log_dg_) << "Cannot find rlp in cache ";
      }
      tr = std::make_shared<std::pair<Transaction, taraxa::bytes>>(
          std::make_pair(*t, exist ? trx_rlp : t->rlp(true)));
    } else {  // search from db
      tr = db_->getTransactionExt(hash);
    }
    thisThreadSleepForMilliSeconds(1);
    counter++;
    if (counter % 10000 == 0) {
      LOG(log_wr_) << "Keep waiting transaction " << hash;
    }
  }
  return tr;
}
// Received block means some trx might be packed by others
bool TransactionManager::saveBlockTransactionAndDeduplicate(
    DagBlock const &blk, std::vector<Transaction> const &some_trxs) {
  vec_trx_t const &all_block_trx_hashes = blk.getTrxs();
  if (all_block_trx_hashes.empty()) {
    return true;
  }
  uint32_t saved_trx_count = 0;
  std::set<trx_hash_t> known_trx_hashes(all_block_trx_hashes.begin(),
                                        all_block_trx_hashes.end());

  if (!some_trxs.empty()) {
    auto trx_batch = db_->createWriteBatch();
    for (auto const &trx : some_trxs) {
      db_->addTransactionToBatch(trx, trx_batch);
      known_trx_hashes.erase(trx.getHash());
    }
    db_->commitWriteBatch(trx_batch);
  }

  bool all_transactions_saved = true;
  trx_hash_t missing_trx;
  for (auto const &trx : known_trx_hashes) {
    auto status = db_->getTransactionStatus(trx);
    if (status == TransactionStatus::not_seen) {
      all_transactions_saved = false;
      missing_trx = trx;
      break;
    }
  }

  if (all_transactions_saved) {
    uLock lock(mu_for_transactions_);
    auto trx_batch = db_->createWriteBatch();
    for (auto const &trx : all_block_trx_hashes) {
      auto status = db_->getTransactionStatus(trx);
      if (status != TransactionStatus::in_block) {
        if (status == TransactionStatus::in_queue_unverified) {
          if (!verifyTransaction(db_->getTransactionExt(trx)->first)) {
            LOG(log_er_) << full_node_.lock()->getAddress()
                         << " Block contains invalid transaction " << trx;
            return false;
          }
        }
        trx_count_.fetch_add(1);
        db_->addTransactionStatusToBatch(trx_batch, trx,
                                         TransactionStatus::in_block);
      }
      auto trx_count = trx_count_.load();
      db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count, trx_batch);
      db_->commitWriteBatch(trx_batch);
    }
  } else {
    LOG(log_er_) << full_node_.lock()->getAddress()
                 << " Missing transaction - FAILED block verification "
                 << missing_trx;
  }

  return all_transactions_saved;
}

bool TransactionManager::insertTrx(Transaction const &trx,
                                   taraxa::bytes const &trx_serialized,
                                   bool verify) {
  bool verified = false;
  auto hash = trx.getHash();
  db_->saveTransaction(trx);
  bool ret = false;

  if (conf_.max_transaction_queue_warn > 0 ||
      conf_.max_transaction_queue_drop > 0) {
    auto queue_size = trx_qu_.getTransactionQueueSize();
    if (conf_.max_transaction_queue_drop >
        queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Trx: " << hash
                   << "skipped, queue too large. Unverified queue: "
                   << queue_size.first
                   << "; Verified queue: " << queue_size.second
                   << "; Limit: " << conf_.max_transaction_queue_drop;
      return false;
    } else if (conf_.max_transaction_queue_warn >
               queue_size.first + queue_size.second) {
      LOG(log_wr_) << "Warning: queue large. Unverified queue: "
                   << queue_size.first
                   << "; Verified queue: " << queue_size.second
                   << "; Limit: " << conf_.max_transaction_queue_drop;
      return false;
    }
  }

  if (verify) {
    verified = (mode_ == VerifyMode::skip_verify_sig) || verifyTransaction(trx);
  }
  if (!verify || verified) {
    uLock lock(mu_for_transactions_);
    auto status = db_->getTransactionStatus(hash);
    if (status == TransactionStatus::not_seen) {
      if (verify) {
        db_->saveTransactionStatus(hash, TransactionStatus::in_queue_verified);
      } else {
        db_->saveTransactionStatus(hash,
                                   TransactionStatus::in_queue_unverified);
      }
      lock.unlock();
      trx_qu_.insert(trx, verify);
      auto node = full_node_.lock();
      if (node) {
        node->newPendingTransaction(trx.getHash());
      }
      ret = true;
    } else {
      if (status == TransactionStatus::in_queue_verified ||
          status == TransactionStatus::in_queue_unverified) {
        LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue. " << std::endl;
      } else if (status == TransactionStatus::in_block) {
        LOG(log_nf_) << "Trx: " << hash << "skip, seen in db. " << std::endl;
      } else if (status == TransactionStatus::invalid) {
        LOG(log_nf_) << "Trx: " << hash << "skip, seen but invalid. "
                     << std::endl;
      }
    }
  }
  return ret;
}

/**
 * This is for block proposer
 * Few steps:
 * 1. get a snapshot (move) of verified queue (lock)
 *	  now, verified trxs can include (A) unpacked ,(B) packed by other ,(C)
 *old trx that only seen in db
 * 2. write A, B to database, of course C will be rejected (already exist in
 *database)
 * 3. propose transactions for block A
 * 4. update A, B and C status to seen_in_db
 */
void TransactionManager::packTrxs(vec_trx_t &to_be_packed_trx,
                                  DagFrontier &frontier,
                                  uint16_t max_trx_to_pack) {
  to_be_packed_trx.clear();
  std::list<Transaction> list_trxs;

  frontier = dag_frontier_;
  LOG(log_dg_) << getFullNodeAddress()
               << " Get frontier with pivot: " << frontier.pivot
               << " tips: " << frontier.tips;

  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot(max_trx_to_pack);

  bool changed = false;
  auto trx_batch = db_->createWriteBatch();
  std::vector<trx_hash_t> trxs_written_to_db;
  {
    uLock lock(mu_for_transactions_);
    for (auto const &i : verified_trx) {
      trx_hash_t const &hash = i.first;
      Transaction const &trx = i.second;
      auto status = db_->getTransactionStatus(hash);
      if (status == TransactionStatus::in_queue_verified) {
        // Skip if transaction is already in existing block
        db_->addTransactionStatusToBatch(trx_batch, hash,
                                         TransactionStatus::in_block);
        trx_count_.fetch_add(1);
        changed = true;
        LOG(log_dg_) << "Trx: " << hash << " ready to pack" << std::endl;
        // update transaction_status
        list_trxs.push_back(trx);
      }
    }

    if (changed) {
      auto trx_count = trx_count_.load();
      db_->addStatusFieldToBatch(StatusDbField::TrxCount, trx_count, trx_batch);
      db_->commitWriteBatch(trx_batch);
    }
  }

  if (list_trxs.size() == 0) {
    return;
  }

  // sort trx based on sender and nonce
  list_trxs.sort(trxComp);
  auto orig_size = list_trxs.size();

  for (auto const &t : list_trxs) {
    to_be_packed_trx.emplace_back(t.getHash());
  }

  auto full_node = full_node_.lock();
  if (full_node) {
    // Need to update pivot incase a new period is confirmed
    std::vector<std::string> ghost;
    full_node->getGhostPath(ghost);
    vec_blk_t gg;
    for (auto const &t : ghost) {
      gg.emplace_back(blk_hash_t(t));
    }
    for (auto const &g : gg) {
      if (g == frontier.pivot) {  // pivot does not change
        break;
      }
      for (auto &t : frontier.tips) {
        if (g == t) {
          std::swap(frontier.pivot, t);
          LOG(log_si_) << getFullNodeAddress()
                       << " Swap frontier with pivot: " << dag_frontier_.pivot
                       << " tips: " << frontier.pivot;
          break;
        }
      }
    }
  }
}

bool TransactionManager::verifyBlockTransactions(
    DagBlock const &blk, std::vector<Transaction> const &trxs) {
  bool invalidTransaction = false;
  for (auto const &trx : trxs) {
    auto valid = verifyTransaction(trx);
    if (!valid) {
      invalidTransaction = true;
      LOG(log_er_) << "Invalid transaction " << trx.getHash().toString();
    }
  }
  if (invalidTransaction) {
    return false;
  }

  // Save the transaction that came with the block together with the
  // transactions that are in the queue. This will update the transaction
  // status as well and remove the transactions from the queue
  bool transactionsSave = saveBlockTransactionAndDeduplicate(blk, trxs);
  if (!transactionsSave) {
    LOG(log_er_) << "Block " << blk.getHash() << " has missing transactions ";
    return false;
  }
  return true;
}

void TransactionManager::updateNonce(DagBlock const &blk,
                                     DagFrontier const &frontier) {
  auto full_node = full_node_.lock();
  if (!full_node) {
    return;
  }
  uLock lock(mu_for_nonce_table_);
  for (auto const &t : blk.getTrxs()) {
    auto trx = getTransaction(t);
    assert(trx);
    auto trx_sender = trx->first.getSender();
    auto trx_hash = trx->first.getHash();
    auto [prev_nonce, exist] = accs_nonce_.get(trx_sender);
    auto trx_nonce = trx->first.getNonce();
    auto new_nonce = trx_nonce > prev_nonce ? trx_nonce : prev_nonce;
    accs_nonce_.update(trx_sender, new_nonce);
  }

  dag_frontier_ = frontier;
  LOG(log_dg_) << getFullNodeAddress() << " Update nonce of block "
               << blk.getHash() << " frontier: " << frontier.pivot
               << " tips: " << frontier.tips
               << " dag_frontier: " << dag_frontier_.pivot
               << " dag_tips: " << dag_frontier_.tips;
}

addr_t TransactionManager::getFullNodeAddress() const {
  auto full_node = full_node_.lock();
  if (full_node) {
    return full_node->getAddress();
  } else {
    return addr_t();
  }
}

}  // namespace taraxa
