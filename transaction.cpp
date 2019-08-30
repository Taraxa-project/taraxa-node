#include "transaction.hpp"
#include <grpcpp/server_builder.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <utility>
#include "full_node.hpp"

namespace taraxa {

using namespace taraxa_grpc;

Transaction::Transaction(stream &strm) { deserialize(strm); }

Transaction::Transaction(string const &json) {
  try {
    boost::property_tree::ptree doc = strToJson(json);
    hash_ = trx_hash_t(doc.get<string>("hash"));
    type_ = toEnum<Transaction::Type>(doc.get<uint8_t>("type"));
    nonce_ = val_t(doc.get<string>("nonce"));
    value_ = val_t(doc.get<string>("value"));
    gas_price_ = val_t(doc.get<string>("gas_price"));
    gas_ = val_t(doc.get<string>("gas"));
    sig_ = sig_t(doc.get<string>("sig"));
    receiver_ = addr_t(doc.get<string>("receiver"));
    string data = doc.get<string>("data");
    data_ = str2bytes(data);
    if (!sig_.isZero()) {
      dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
      if (sig_struct.isValid()) {
        vrs_ = sig_struct;
      }
    }
    chain_id_ = doc.get<int8_t>("chain_id");
    cached_sender_ = addr_t(doc.get<string>("sender"));
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    assert(false);
  }
}

Transaction::Transaction(bytes const &_rlp) {
  dev::RLP const rlp(_rlp);
  if (!rlp.isList())
    throw std::invalid_argument("transaction RLP must be a list");

  nonce_ = rlp[0].toInt<dev::u256>();
  gas_price_ = rlp[1].toInt<dev::u256>();
  gas_ = rlp[2].toInt<dev::u256>();
  type_ = rlp[3].isEmpty() ? taraxa::Transaction::Type::Create
                           : taraxa::Transaction::Type::Call;
  receiver_ = rlp[3].isEmpty()
                  ? dev::Address()
                  : rlp[3].toHash<dev::Address>(dev::RLP::VeryStrict);
  value_ = rlp[4].toInt<dev::u256>();

  if (!rlp[5].isData())
    throw std::invalid_argument("transaction data RLP must be an array");

  data_ = rlp[5].toBytes();

  int const v = rlp[6].toInt<int>();
  h256 const r = rlp[7].toInt<dev::u256>();
  h256 const s = rlp[8].toInt<dev::u256>();

  if (!r && !s) {
    chain_id_ = v;
    sig_ = dev::SignatureStruct{r, s, 0};
  } else {
    if (v > 36)
      chain_id_ = (v - 35) / 2;
    else if (v == 27 || v == 28)
      chain_id_ = -4;
    else
      throw std::invalid_argument("InvalidSignature()");

    sig_ = dev::SignatureStruct{r, s,
                                static_cast<::byte>(v - (chain_id_ * 2 + 35))};
  }
  dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
  if (sig_struct.isValid()) {
    vrs_ = sig_struct;
  }

  if (rlp.itemCount() > 9)
    throw std::invalid_argument("too many fields in the transaction RLP");
  updateHash();
}

bool Transaction::serialize(stream &strm) const {
  bool ok = true;
  ok &= write(strm, hash_);
  ok &= write(strm, type_);
  ok &= write(strm, nonce_);
  ok &= write(strm, value_);
  ok &= write(strm, gas_price_);
  ok &= write(strm, gas_);
  ok &= write(strm, receiver_);
  ok &= write(strm, sig_);
  std::size_t byte_size = data_.size();
  ok &= write(strm, byte_size);
  for (auto i = 0; i < byte_size; ++i) {
    ok &= write(strm, data_[i]);
  }
  assert(ok);
  return ok;
}

bool Transaction::deserialize(stream &strm) {
  bool ok = true;
  ok &= read(strm, hash_);
  ok &= read(strm, type_);
  ok &= read(strm, nonce_);
  ok &= read(strm, value_);
  ok &= read(strm, gas_price_);
  ok &= read(strm, gas_);
  ok &= read(strm, receiver_);
  ok &= read(strm, sig_);
  std::size_t byte_size;
  ok &= read(strm, byte_size);
  data_.resize(byte_size);
  for (auto i = 0; i < byte_size; ++i) {
    ok &= read(strm, data_[i]);
  }
  if (!sig_.isZero()) {
    dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
    if (sig_struct.isValid()) {
      vrs_ = sig_struct;
    }
  }

  assert(ok);
  return ok;
}
string Transaction::getJsonStr() const {
  boost::property_tree::ptree tree;
  tree.put("hash", hash_.toString());
  tree.put("type", asInteger(type_));
  tree.put("nonce", nonce_.str());
  tree.put("value", value_.str());
  tree.put("gas_price", toString(gas_price_));
  tree.put("gas", toString(gas_));
  tree.put("sig", sig_.toString());
  tree.put("receiver", receiver_.toString());
  tree.put("data", bytes2str(data_));
  tree.put("chain_id", chain_id_);
  tree.put("sender", cached_sender_.toString());
  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}
void Transaction::sign(secret_t const &sk) {
  if (!sig_) {
    sig_ = dev::sign(sk, sha3(false));
    dev::SignatureStruct sig_struct = *(dev::SignatureStruct const *)&sig_;
    if (sig_struct.isValid()) {
      vrs_ = sig_struct;
    }
  }
  sender();
  updateHash();
}

bool Transaction::verifySig() const {
  if (!sig_) return false;
  auto msg = sha3(false);
  auto pk = dev::recover(sig_, msg);
  return dev::verify(pk, sig_, msg);
}
addr_t Transaction::sender() const {
  if (!cached_sender_) {
    if (!sig_) {
      return addr_t{};
    }
    auto p = dev::recover(sig_, sha3(false));
    assert(p);
    cached_sender_ =
        dev::right160(dev::sha3(dev::bytesConstRef(p.data(), sizeof(p))));
  }
  return cached_sender_;
}
void Transaction::streamRLP(dev::RLPStream &s, bool include_sig,
                            bool _forEip155hash) const {
  if (type_ == Transaction::Type::Null) return;
  s.appendList(include_sig || _forEip155hash ? 9 : 6);
  s << nonce_ << gas_price_ << gas_;
  if (type_ == Transaction::Type::Call) {
    s << receiver_;
  } else {
    s << "";
  }
  s << value_ << data_;
  if (include_sig) {
    assert(vrs_);
    if (hasZeroSig()) {
      s << chain_id_;
    } else {
      int const v_offset = chain_id_ * 2 + 35;
      s << (vrs_->v + v_offset);
    }
    s << (dev::u256)vrs_->r << (dev::u256)vrs_->s;
  } else if (_forEip155hash)
    s << chain_id_ << 0 << 0;
}

bytes Transaction::rlp(bool include_sig) const {
  dev::RLPStream s;
  streamRLP(s, include_sig, chain_id_ > 0 && !include_sig);
  return s.out();
};

blk_hash_t Transaction::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

void TransactionQueue::start() {
  if (!stopped_) return;
  stopped_ = false;
  verifiers_.clear();
  for (auto i = 0; i < num_verifiers_; ++i) {
    LOG(log_nf_) << "Create Transaction verifier ... " << std::endl;
    verifiers_.emplace_back([this]() { verifyQueuedTrxs(); });
  }
  assert(num_verifiers_ == verifiers_.size());
}

void TransactionQueue::stop() {
  if (stopped_) return;
  stopped_ = true;
  cond_for_unverified_qu_.notify_all();
  for (auto &t : verifiers_) {
    t.join();
  }
}

bool TransactionQueue::insert(Transaction trx, bool critical) {
  trx_hash_t hash = trx.getHash();
  auto status = trx_status_.get(hash);
  bool ret = false;
  std::list<Transaction>::iterator iter;
  if (status.second == false) {  // never seen before
    ret = trx_status_.insert(hash, TransactionStatus::in_queue_unverified);
    if (ret) {
      {
        uLock lock(shared_mutex_for_queued_trxs_);
        iter = trx_buffer_.insert(trx_buffer_.end(), trx);
        assert(iter != trx_buffer_.end());
        queued_trxs_[trx.getHash()] = iter;
      }
      {
        uLock lock(shared_mutex_for_unverified_qu_);
        if (critical) {
          unverified_hash_qu_.emplace_front(std::make_pair(hash, iter));
        } else {
          unverified_hash_qu_.emplace_back(std::make_pair(hash, iter));
        }
      }
      cond_for_unverified_qu_.notify_one();
      LOG(log_nf_) << "Trx: " << hash << " inserted. " << std::endl;
    } else {
      // If ret is false status was just changed by another thread so ask for it
      // again
      status = trx_status_.get(hash);
    }
  }
  if (status.second == true) {
    if (status.first == TransactionStatus::in_queue_unverified) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue (unverified). "
                   << std::endl;
    } else if (status.first == TransactionStatus::in_queue_verified) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue (verified). "
                   << std::endl;
    } else if (status.first == TransactionStatus::in_block) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen in db. " << std::endl;
    } else if (status.first == TransactionStatus::invalid) {
      LOG(log_nf_) << "Trx: " << hash << "skip, seen but invalid. "
                   << std::endl;
    }
  }
  return ret;
}

void TransactionQueue::verifyQueuedTrxs() {
  while (!stopped_) {
    // Transaction utrx;
    std::pair<trx_hash_t, listIter> item;
    {
      uLock lock(shared_mutex_for_unverified_qu_);
      while (unverified_hash_qu_.empty() && !stopped_) {
        cond_for_unverified_qu_.wait(lock);
      }
      if (stopped_) {
        LOG(log_nf_) << "Transaction verifier stopped ... " << std::endl;
        return;
      }
      item = unverified_hash_qu_.front();
      unverified_hash_qu_.pop_front();
    }
    try {
      trx_hash_t hash = item.first;
      Transaction &trx = *(item.second);
      // verify and put the transaction to verified queue
      bool valid = false;
      if (mode_ == VerifyMode::skip_verify_sig) {
        valid = true;
      } else {
        valid = trx.verifySig();
      }
      // mark invalid
      if (!valid) {
        trx_status_.update(trx.getHash(), TransactionStatus::invalid);
        {
          uLock lock(shared_mutex_for_queued_trxs_);
          trx_buffer_.erase(queued_trxs_[hash]);
          queued_trxs_.erase(hash);
        }
        LOG(log_wr_) << "Trx: " << hash << "invalid. " << std::endl;

      } else {
        // push to verified qu
        LOG(log_nf_) << "Trx: " << hash << " verified OK." << std::endl;
        bool ret = trx_status_.update(trx.getHash(),
                                      TransactionStatus::in_queue_verified,
                                      TransactionStatus::in_queue_unverified);
        if (ret) {
          uLock lock(shared_mutex_for_verified_qu_);
          verified_trxs_[hash] = item.second;
          new_verified_transactions = true;
        }
      }
    } catch (...) {
      assert(false);
    }
  }
}

// The caller is responsible for storing the transaction to db!
std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::removeBlockTransactionsFromQueue(
    vec_trx_t const &all_block_trxs) {
  std::unordered_map<trx_hash_t, Transaction> result;
  {
    std::vector<trx_hash_t> removed_trx;
    for (auto const &trx : all_block_trxs) {
      {
        upgradableLock lock(shared_mutex_for_verified_qu_);
        auto vrf_trx = verified_trxs_.find(trx);
        if (vrf_trx != verified_trxs_.end()) {
          result[vrf_trx->first] = *(vrf_trx->second);
          removed_trx.emplace_back(trx);
          {
            upgradeLock locked(lock);
            verified_trxs_.erase(trx);
          }
        }
      }
    }
    // TODO: can also remove from unverified_hash_qu_

    // clear trx_buffer
    {
      uLock lock(shared_mutex_for_queued_trxs_);
      for (auto const &t : removed_trx) {
        trx_buffer_.erase(queued_trxs_[t]);
        queued_trxs_.erase(t);
      }
    }
  }
  return result;
}

std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::getNewVerifiedTrxSnapShot(bool onlyNew) {
  std::unordered_map<trx_hash_t, Transaction> verified_trxs;
  if (new_verified_transactions || !onlyNew) {
    if (onlyNew) new_verified_transactions = false;
    sharedLock lock(shared_mutex_for_verified_qu_);
    for (auto const &trx : verified_trxs_) {
      verified_trxs[trx.first] = *(trx.second);
    }
    LOG(log_dg_) << "Get: " << verified_trxs.size() << " verified trx out. "
                 << std::endl;
  }
  return verified_trxs;
}

// search from queued_trx_
std::shared_ptr<Transaction> TransactionQueue::getTransaction(
    trx_hash_t const &hash) const {
  {
    sharedLock lock(shared_mutex_for_queued_trxs_);
    auto it = queued_trxs_.find(hash);
    if (it != queued_trxs_.end()) {
      return std::make_shared<Transaction>(*(it->second));
    }
  }
  return nullptr;
}

std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::moveVerifiedTrxSnapShot() {
  std::unordered_map<trx_hash_t, Transaction> res;
  {
    uLock lock(shared_mutex_for_verified_qu_);
    for (auto const &trx : verified_trxs_) {
      res[trx.first] = *(trx.second);
    }
    verified_trxs_.clear();
  }
  {
    uLock lock(shared_mutex_for_queued_trxs_);
    for (auto const &t : res) {
      trx_buffer_.erase(queued_trxs_[t.first]);
      queued_trxs_.erase(t.first);
    }
  }
  if (res.size() > 0) {
    LOG(log_dg_) << "Copy " << res.size() << " verified trx. " << std::endl;
  }
  return std::move(res);
}

unsigned long TransactionQueue::getVerifiedTrxCount() {
  sharedLock lock(shared_mutex_for_verified_qu_);
  return verified_trxs_.size();
}

void TransactionManager::start() {
  if (!stopped_) return;
  if (!node_.lock()) {
    LOG(log_wr_) << "FullNode is not set ...";
    assert(db_trxs_);
  } else {
    assert(!db_trxs_);
    db_trxs_ = node_.lock()->getTrxsDB();
  }
  trx_qu_.start();

  stopped_ = false;
}

void TransactionManager::stop() {
  if (stopped_) return;
  db_trxs_ = nullptr;
  trx_qu_.stop();
  stopped_ = true;
}

std::unordered_map<trx_hash_t, Transaction>
TransactionManager::getNewVerifiedTrxSnapShot(bool onlyNew) {
  return trx_qu_.getNewVerifiedTrxSnapShot(onlyNew);
}

unsigned long TransactionManager::getTransactionStatusCount() const {
  return trx_status_.size();
}

std::shared_ptr<Transaction> TransactionManager::getTransaction(
    trx_hash_t const &hash) const {
  // Check the status
  std::shared_ptr<Transaction> tr;
  // Loop needed because moving transactions from queue to database is not
  // secure Probably a better fix is to have transactions saved to the
  // database first and only then removed from the queue
  while (tr == nullptr) {
    auto status = trx_status_.get(hash);
    if (status.second) {
      if (status.first == TransactionStatus::in_queue_verified) {
        tr = trx_qu_.getTransaction(hash);
      } else if (status.first == TransactionStatus::in_queue_unverified) {
        LOG(log_er_) << "Why do you query unverified trx??";
        assert(1);
      } else if (status.first == TransactionStatus::in_block) {
        std::string json = db_trxs_->get(hash.toString());
        if (!json.empty()) {
          tr = std::make_shared<Transaction>(json);
        }
      } else {
        std::string json = db_trxs_->get(hash.toString());
        if (!json.empty()) {
          tr = std::make_shared<Transaction>(json);
        }
        break;
      }
    } else
      break;
  }
  return tr;
}
// Received block means some trx might be packed by others
bool TransactionManager::saveBlockTransactionAndDeduplicate(
    vec_trx_t const &all_block_trx_hashes,
    std::vector<Transaction> const &some_trxs) {
  if (all_block_trx_hashes.empty()) {
    return true;
  }
  // First step: Save and update status for transactions that were sent
  // together with the block some_trxs might not full trxs in the block (for
  // syncing purpose)
  if (!some_trxs.empty()) {
    for (auto const &trx : some_trxs) {
      db_trxs_->put(trx.getHash().toString(), trx.getJsonStr());
      trx_status_.update(trx.getHash(), TransactionStatus::in_block);
    }
    db_trxs_->commit();
  }

  // Second step: Remove from the queue any transaction that is part of the
  // block Verify that all transactions are saved in the database If all
  // transactions are not available within 10 seconds fail the block
  // verification
  bool all_transactions_saved = true;
  unsigned int delay = 0;
  vec_trx_t unsaved_trx;
  while (delay < 10000) {
    {
      auto removed_trx =
          trx_qu_.removeBlockTransactionsFromQueue(all_block_trx_hashes);
      for (auto const &trx : removed_trx) {
        db_trxs_->put(trx.first.toString(), trx.second.getJsonStr());
        trx_status_.update(trx.first, TransactionStatus::in_block);
      }
      db_trxs_->commit();
    }
    all_transactions_saved = true;
    unsaved_trx.clear();
    for (auto const &trx : all_block_trx_hashes) {
      auto res = trx_status_.get(trx);
      if (res.second == false || res.first != TransactionStatus::in_block) {
        all_transactions_saved = false;
        unsaved_trx.emplace_back(trx);
      }
    }
    // Only if all transactions are saved in the db can we verify a new block
    if (all_transactions_saved) break;
    // Getting here should be a very rare case where in a racing condition
    // block was processed before the transactions
    thisThreadSleepForMilliSeconds(10);
    delay += 10;
  }
  if (!all_transactions_saved) {
    LOG(log_er_) << "Missing transactions " << unsaved_trx;
  }
  return all_transactions_saved;
}

bool TransactionManager::insertTrx(Transaction trx, bool critical) {
  bool ret = false;
  if (trx_qu_.insert(trx, critical)) {
    ret = true;
    auto node = node_.lock();
    if (node) {
      node->newPendingTransaction(trx.getHash());
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
void TransactionManager::packTrxs(vec_trx_t &to_be_packed_trx) {
  to_be_packed_trx.clear();

  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot();
  bool changed = false;
  for (auto const &i : verified_trx) {
    trx_hash_t const &hash = i.first;
    Transaction const &trx = i.second;

    // Skip if transaction is already in existing block
    if (!db_trxs_->put(hash.toString(), trx.getJsonStr())) {
      continue;
    }
    changed = true;
    TransactionStatus status;
    bool exist;
    std::tie(status, exist) = trx_status_.get(hash);
    assert(exist);
    LOG(log_dg_) << "Trx: " << hash << " ready to pack" << std::endl;
    // update transaction_status
    trx_status_.update(hash, TransactionStatus::in_block);
    to_be_packed_trx.emplace_back(i.first);
  }
  if (changed) {
    db_trxs_->commit();
  }
}

bool TransactionManager::verifyBlockTransactions(
    DagBlock const &blk, std::vector<Transaction> const &trxs) {
  bool invalidTransaction = false;
  for (auto const &trx : trxs) {
    auto valid = trx.verifySig();
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
  bool transactionsSave =
      saveBlockTransactionAndDeduplicate(blk.getTrxs(), trxs);
  if (!transactionsSave) {
    LOG(log_er_) << "Block " << blk.getHash() << " has missing transactions ";
    return false;
  }
  return true;
}

}  // namespace taraxa
