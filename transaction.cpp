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
  sig_ = dev::sign(sk, sha3(false));
}

bool Transaction::verifySig() {
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
    LOG(logger_) << "Create Transaction verifier ... " << std::endl;
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
    ret = trx_status_.insert(hash, TransactionStatus::seen_in_queue);
    uLock lock(mutex_for_unverified_qu_);
    unverified_qu_.emplace_back(trx);
    cond_for_unverified_qu_.notify_one();
    LOG(logger_) << "Trx: " << hash << "inserted. " << std::endl;
    ret = true;
  } else if (status.first ==
             TransactionStatus::
                 unseen_but_already_packed_by_others) {  // updated by other
                                                         // blocks
    ret = trx_status_.compareAndSwap(
        hash, TransactionStatus::unseen_but_already_packed_by_others,
        TransactionStatus::seen_in_queue_but_already_packed_by_others);
    uLock lock(mutex_for_unverified_qu_);
    unverified_qu_.emplace_back(trx);
    cond_for_unverified_qu_.notify_one();
    LOG(logger_) << "Trx: " << hash
                 << "already packed by others, but still enqueue. "
                 << std::endl;
  } else if (status.first == TransactionStatus::seen_in_queue) {
    LOG(logger_) << "Trx: " << hash << "skip, seen in queue. " << std::endl;
  } else if (status.first == TransactionStatus::seen_in_db) {
    LOG(logger_) << "Trx: " << hash << "skip, seen in db. " << std::endl;
  } else if (status.first == TransactionStatus::seen_but_invalid) {
    LOG(logger_) << "Trx: " << hash << "skip, seen but invalid. " << std::endl;
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
        LOG(logger_) << "Transaction verifier stopped ... " << std::endl;
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
        trx_status_.compareAndSwap(trx.getHash(),
                                   TransactionStatus::seen_in_queue,
                                   TransactionStatus::seen_but_invalid);
        LOG(logger_) << "Trx: " << hash << "invalid. " << std::endl;

      } else {
        // push to verified qu
        LOG(logger_) << "Trx: " << hash << "verified OK. " << std::endl;

        uLock lock(mutex_for_verified_qu_);
        verified_trxs_[trx.getHash()] = trx;
      }
    } catch (...) {
    }
  }
}
std::unordered_map<trx_hash_t, Transaction>
TransactionQueue::moveVerifiedTrxSnapShot() {
  uLock lock(mutex_for_verified_qu_);
  auto verified_trxs = std::move(verified_trxs_);
  assert(verified_trxs_.empty());
  LOG(logger_) << "Move: " << verified_trxs.size() << " verified trx out. "
               << std::endl;
  return std::move(verified_trxs);
}
bool TransactionManager::insertTrx(Transaction trx) {
  bool ret = false;
  if (trx_qu_.insert(trx)) {
    trx_counter_.fetch_add(1);
    cond_for_pack_trx_.notify_one();
    ret = true;
  }
  return ret;
}
void TransactionManager::setPackedTrxFromBlockHash(blk_hash_t blk) {}
void TransactionManager::setPackedTrxFromBlock(DagBlock const &blk) {
  vec_trx_t trxs = blk.getTrxs();
  TransactionStatus status;
  bool exist;
  for (auto const &t : trxs) {
    std::tie(status, exist) = trx_status_.get(t);
    if (!exist) {
      trx_status_.insert(
          t, TransactionStatus::unseen_but_already_packed_by_others);
      LOG(logger_) << "Blk->Trx : " << t
                   << " unseen but already packed by others" << std::endl;

    } else if (status == TransactionStatus::seen_in_queue) {
      trx_status_.compareAndSwap(
          t, TransactionStatus::seen_in_queue,
          TransactionStatus::seen_in_queue_but_already_packed_by_others);
      LOG(logger_) << "Blk->Trx : " << t
                   << " seen in queue but already packed by others"
                   << std::endl;
    } else if (status == TransactionStatus::seen_but_invalid) {
      LOG(logger_) << "Blk->Trx : " << t << " skip, seen and invalid"
                   << std::endl;
    } else if (status ==
               TransactionStatus::seen_in_queue_but_already_packed_by_others) {
      LOG(logger_) << "Blk->Trx : " << t << " skip, seen packed by others."
                   << std::endl;
    } else if (status == TransactionStatus::seen_in_db) {
      LOG(logger_) << "Blk->Trx : " << t << " skip, seen in db." << std::endl;
    }
  }
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
  uLock pack_lock(mutex_for_pack_trx_);
  while (!stopped_ && trx_counter_ < rate_limiter_) {
    cond_for_pack_trx_.wait(pack_lock);
  }
  if (stopped_) return;
  // reset counter
  trx_counter_.store(0);
  auto verified_trx = trx_qu_.moveVerifiedTrxSnapShot();
  std::vector<trx_hash_t> exist_in_db;
  std::vector<trx_hash_t> packed_by_others;
  to_be_packed_trx.clear();
  uLock lock(mutex_);
  for (auto const &i : verified_trx) {
    trx_hash_t const &hash = i.first;
    Transaction const &trx = i.second;
    if (!db_trxs_->put(hash.toString(), trx.getJsonStr())) {
      exist_in_db.emplace_back(i.first);
    }
    TransactionStatus status;
    bool exist;
    std::tie(status, exist) = trx_status_.get(hash);
    assert(exist);
    if (status ==
        TransactionStatus::seen_in_queue_but_already_packed_by_others) {
      packed_by_others.emplace_back(hash);
      LOG(logger_) << "Trx: " << hash << " already packed by others"
                   << std::endl;
    } else if (status == TransactionStatus::seen_in_queue) {
      to_be_packed_trx.emplace_back(i.first);
      LOG(logger_) << "Trx: " << hash << " ready to pack" << std::endl;
    } else {
      LOG(logger_) << "Warning! Trx: " << hash << " status "
                   << asInteger(status) << std::endl;
      LOG(logger_dbg_) << "Warning! Trx: " << hash << " status "
                       << asInteger(status) << std::endl;
      assert(true);
    }
  }
  // update transaction_status
  bool res;
  for (auto const &t : exist_in_db) {
    res = trx_status_.insert(t, TransactionStatus::seen_in_db);
    if (!res) {
      TransactionStatus status;
      bool exist;
      std::tie(status, exist) = trx_status_.get(t);
      res = (status == TransactionStatus::seen_in_db);
      assert(res);
    }
  }
  for (auto const &t : packed_by_others) {
    res = trx_status_.compareAndSwap(
        t, TransactionStatus::seen_in_queue_but_already_packed_by_others,
        TransactionStatus::seen_in_db);
    assert(res);
  }
  for (auto const &t : to_be_packed_trx) {
    res = trx_status_.compareAndSwap(t, TransactionStatus::seen_in_queue,
                                     TransactionStatus::seen_in_db);
    assert(res);
  }
}

}  // namespace taraxa