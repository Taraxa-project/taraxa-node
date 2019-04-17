#include "transaction.hpp"
#include <grpcpp/server_builder.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <utility>

namespace taraxa {

using namespace taraxa_grpc;
Transaction::Transaction(stream &strm) { deserialize(strm); }

Transaction::Transaction(string const &json) {
  try {
    boost::property_tree::ptree doc = strToJson(json);
    hash_ = trx_hash_t(doc.get<string>("hash"));
    type_ = toEnum<Transaction::Type>(doc.get<uint8_t>("type"));
    nonce_ = doc.get<uint64_t>("nonce");
    value_ = doc.get<uint64_t>("value");
    gas_price_ = val_t(doc.get<string>("gas_price"));
    gas_ = val_t(doc.get<string>("gas"));
    sig_ = sig_t(doc.get<string>("sig"));
    receiver_ = addr_t(doc.get<string>("receiver"));
    string data = doc.get<string>("data");
    data_ = str2bytes(data);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
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
  assert(ok);
  return ok;
}
string Transaction::getJsonStr() const {
  boost::property_tree::ptree tree;
  tree.put("hash", hash_.toString());
  tree.put("type", asInteger(type_));
  tree.put("nonce", nonce_);
  tree.put("value", value_);
  tree.put("gas_price", gas_price_.toString());
  tree.put("gas", gas_.toString());
  tree.put("sig", sig_.toString());
  tree.put("receiver", receiver_.toString());
  tree.put("data", bytes2str(data_));
  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}
void Transaction::sign(secret_t const &sk) {
  if (!sig_) {
    sig_ = dev::sign(sk, sha3(false));
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
void Transaction::streamRLP(dev::RLPStream &s, bool include_sig) const {
  if (type_ == Transaction::Type::Null) return;
  s.appendList(include_sig ? 7 : 6);
  s << nonce_ << value_ << gas_price_ << gas_;
  if (type_ == Transaction::Type::Call) {
    s << receiver_;
  } else {
    s << "";
  }
  s << data_;
  if (include_sig) {
    s << sig_;
  }
}

bytes Transaction::rlp(bool include_sig) const {
  dev::RLPStream s;
  streamRLP(s, include_sig);
  return s.out();
};

blk_hash_t Transaction::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

void TransactionQueue::start() {
  if (!stopped_) return;
  stopped_ = false;
  for (auto i = 0; i < num_verifiers_; ++i) {
    LOG(log_nf_) << "Create Transaction verifier ... " << std::endl;
    verifiers_.emplace_back([this]() { verifyTrx(); });
  }
}

void TransactionQueue::stop() {
  if (stopped_) return;
  stopped_ = true;
  cond_for_unverified_qu_.notify_all();
  for (auto &t : verifiers_) {
    t.join();
  }
}

bool TransactionQueue::insert(Transaction trx) {
  trx_hash_t hash = trx.getHash();
  auto status = trx_status_.get(hash);
  bool ret = false;
  if (status.second == false) {  // never seen before
    ret = trx_status_.insert(hash, TransactionStatus::in_queue);
    uLock lock(mutex_for_unverified_qu_);
    unverified_qu_.emplace_back(trx);
    cond_for_unverified_qu_.notify_one();
    LOG(log_nf_) << "Trx: " << hash << " inserted. " << std::endl;
    ret = true;
  } else if (status.first == TransactionStatus::in_queue) {
    LOG(log_nf_) << "Trx: " << hash << "skip, seen in queue. " << std::endl;
  } else if (status.first == TransactionStatus::in_block) {
    LOG(log_nf_) << "Trx: " << hash << "skip, seen in db. " << std::endl;
  } else if (status.first == TransactionStatus::invalid) {
    LOG(log_nf_) << "Trx: " << hash << "skip, seen but invalid. " << std::endl;
  }

  return ret;
}

void TransactionQueue::verifyTrx() {
  while (!stopped_) {
    Transaction utrx;
    {
      uLock lock(mutex_for_unverified_qu_);
      while (unverified_qu_.empty() && !stopped_) {
        cond_for_unverified_qu_.wait(lock);
      }
      if (stopped_) {
        LOG(log_nf_) << "Transaction verifier stopped ... " << std::endl;
        return;
      }
      utrx = std::move(unverified_qu_.front());
      unverified_qu_.pop_front();
    }
    try {
      Transaction trx = std::move(utrx);
      trx_hash_t hash = trx.getHash();
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
        LOG(log_wr_) << "Trx: " << hash << "invalid. " << std::endl;

      } else {
        // push to verified qu
        LOG(log_nf_) << "Trx: " << hash << " verified OK." << std::endl;

        uLock lock(mutex_for_verified_qu_);
        verified_trxs_[trx.getHash()] = trx;
        new_verified_transactions = true;
      }
    } catch (...) {
    }
  }
}

std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::removeBlockTransactionsFromQueue(
    vec_trx_t const &allBlockTransactions) {
  std::unordered_map<trx_hash_t, Transaction> result;
  while (true) {
    {
      uLock lock(mutex_for_verified_qu_);
      for (auto const &trx : allBlockTransactions) {
        auto fTrx = verified_trxs_.find(trx);
        if (fTrx != verified_trxs_.end()) {
          result[fTrx->first] = fTrx->second;
          verified_trxs_.erase(trx);
        }
      }
    }
    {
      // Check that some of the block transaction is not in the unverified
      // queue, if so wait for it to enter verified queue
      uLock lock(mutex_for_unverified_qu_);
      bool found = false;
      for (auto const &uTrx : unverified_qu_) {
        for (auto const &trx : allBlockTransactions) {
          if (trx == uTrx.getHash()) found = true;
        }
      }
      if (!found) return result;
    }
    thisThreadSleepForMilliSeconds(10);
  }
}

std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::getNewVerifiedTrxSnapShot(bool onlyNew) {
  std::unordered_map<trx_hash_t, Transaction> verified_trxs;
  if (new_verified_transactions || !onlyNew) {
    if (onlyNew) new_verified_transactions = false;
    uLock lock(mutex_for_verified_qu_);
    verified_trxs = verified_trxs_;
    LOG(log_nf_) << "Get: " << verified_trxs.size() << " verified trx out. "
                 << std::endl;
  }
  return verified_trxs;
}

std::shared_ptr<Transaction> TransactionQueue::getTransaction(
    trx_hash_t const &hash) {
  {
    uLock lock(mutex_for_unverified_qu_);
    for (auto trx : unverified_qu_)
      if (trx.getHash() == hash) return std::make_shared<Transaction>(trx);
  }
  {
    uLock lock(mutex_for_verified_qu_);
    for (auto trx : verified_trxs_)
      if (trx.first == hash) return std::make_shared<Transaction>(trx.second);
  }
  return nullptr;
}

std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::moveVerifiedTrxSnapShot() {
  uLock lock(mutex_for_verified_qu_);
  auto verified_trxs = std::move(verified_trxs_);
  assert(verified_trxs_.empty());
  LOG(log_nf_) << "Move: " << verified_trxs.size() << " verified trx out. "
               << std::endl;
  return std::move(verified_trxs);
}

unsigned long TransactionQueue::getVerifiedTrxCount() {
  uLock lock(mutex_for_verified_qu_);
  return verified_trxs_.size();
}

std::unordered_map<trx_hash_t, Transaction>
TransactionManager::getNewVerifiedTrxSnapShot(bool onlyNew) {
  return trx_qu_.getNewVerifiedTrxSnapShot(onlyNew);
}

std::shared_ptr<Transaction> TransactionManager::getTransaction(
    trx_hash_t const &hash) {
  return trx_qu_.getTransaction(hash);
}
// Received block means some trx might be packed by others
bool TransactionManager::saveBlockTransactionsAndUpdateTransactionStatus(
    vec_trx_t const &all_block_trx_hashes, std::vector<Transaction> const &some_trxs) {
  //First step: Save and update status for transactions that were sent together with the block
  //some_trxs might not full trxs in the block (for syncing purpose) 
  for (auto const &trx : some_trxs) {
    db_trxs_->put(trx.getHash().toString(), trx.getJsonStr());
    trx_status_.update(trx.getHash(), TransactionStatus::in_block);
  }

  //Second step: Retrieve trxs which are in the queue but already packed by others and update the status
  for (auto const &trx :
       trx_qu_.removeBlockTransactionsFromQueue(all_block_trx_hashes)) {
    db_trxs_->put(trx.first.toString(), trx.second.getJsonStr());
    trx_status_.update(trx.first, TransactionStatus::in_block);
  }
}

bool TransactionManager::insertTrx(Transaction trx) {
  bool ret = false;
  if (trx_qu_.insert(trx)) {
    cond_for_pack_trx_.notify_one();
    ret = true;
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
  static std::uniform_int_distribution<> d(1, 30000);
  static std::mt19937 gen;
  uLock pack_lock(mutex_for_pack_trx_);
  while (!stopped_ && trx_qu_.getVerifiedTrxCount() < rate_limiter_) {
    cond_for_pack_trx_.wait_for(pack_lock, std::chrono::milliseconds(1000));
    // Rate limiter will make all nodes create parallel blocks at exactly same
    // time, add some random delay to avoid that
    thisThreadSleepForMilliSeconds(d(gen));
    // printf("Trx count: %d\n", trx_qu_.getVerifiedTrxCount());
  }

  if (stopped_) return;
  // reset counter
  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot();
  uLock lock(mutex_);
  for (auto const &i : verified_trx) {
    trx_hash_t const &hash = i.first;
    Transaction const &trx = i.second;
    // Skip if transaction is already in existing block
    if (!db_trxs_->put(hash.toString(), trx.getJsonStr())) {
      trx_status_.insert(hash, TransactionStatus::in_block);
      continue;
    }
    TransactionStatus status;
    bool exist;
    std::tie(status, exist) = trx_status_.get(hash);
    assert(exist);
    LOG(log_nf_) << "Trx: " << hash << " ready to pack" << std::endl;
    // update transaction_status
    trx_status_.update(hash, TransactionStatus::in_block);
    to_be_packed_trx.emplace_back(i.first);
  }
}

}  // namespace taraxa