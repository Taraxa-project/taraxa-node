/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-10-31 16:26:04
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-16 13:10:10
 */
#include "dag_block.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <utility>
#include "dag.hpp"
#include "full_node.hpp"
#include "libdevcore/Log.h"

namespace taraxa {

using std::to_string;

DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips,
                   vec_trx_t trxs, sig_t sig, blk_hash_t hash,
                   addr_t sender) try : pivot_(pivot),
                                        level_(level),
                                        tips_(tips),
                                        trxs_(trxs),
                                        sig_(sig),
                                        hash_(hash),
                                        cached_sender_(sender) {
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
}
DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs) try
    : pivot_(pivot),
      level_(level),
      tips_(tips),
      trxs_(trxs) {
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
}

DagBlock::DagBlock(stream &strm) { deserialize(strm); }
DagBlock::DagBlock(std::string const &json) {
  try {
    boost::property_tree::ptree doc = strToJson(json);
    pivot_ = blk_hash_t(doc.get<std::string>("pivot"));
    level_ = level_t(doc.get<level_t>("level"));
    tips_ = asVector<blk_hash_t, std::string>(doc, "tips");
    trxs_ = asVector<trx_hash_t, std::string>(doc, "trxs");
    sig_ = sig_t(doc.get<std::string>("sig"));
    hash_ = blk_hash_t(doc.get<std::string>("hash"));
    cached_sender_ = addr_t(doc.get<std::string>("sender"));
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
DagBlock::DagBlock(dev::RLP const &_r) {
  std::vector<::byte> blockBytes;
  blockBytes = _r.toBytes();
  taraxa::bufferstream strm(blockBytes.data(), blockBytes.size());
  deserialize(strm);
}
void DagBlock::serializeRLP(dev::RLPStream &s) {
  std::vector<uint8_t> bytes;
  {
    vectorstream strm(bytes);
    serialize(strm);
  }
  s.append(bytes);
}
bool DagBlock::isValid() const {
  return !(pivot_.isZero() && hash_.isZero() && sig_.isZero() &&
           cached_sender_.isZero());
}

std::string DagBlock::getJsonStr() const {
  using boost::property_tree::ptree;

  ptree tree;
  tree.put("pivot", pivot_.toString());
  tree.put("level", std::to_string(level_));
  tree.put_child("tips", ptree());
  auto &tips_array = tree.get_child("tips");
  for (auto const &t : tips_) {
    tips_array.push_back(std::make_pair("", ptree(t.toString().c_str())));
  }

  tree.put_child("trxs", ptree());
  auto &trxs_array = tree.get_child("trxs");
  for (auto const &t : trxs_) {
    trxs_array.push_back(std::make_pair("", ptree(t.toString().c_str())));
  }

  tree.put("sig", sig_.toString());
  tree.put("hash", hash_.toString());
  tree.put("sender", cached_sender_.toString());

  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

bool DagBlock::serialize(stream &strm) const {
  bool ok = true;
  uint32_t num_tips = tips_.size();
  uint32_t num_trxs = trxs_.size();
  ok &= write(strm, num_tips);
  ok &= write(strm, num_trxs);
  ok &= write(strm, pivot_);
  ok &= write(strm, level_);
  for (auto i = 0; i < num_tips; ++i) {
    ok &= write(strm, tips_[i]);
  }
  for (auto i = 0; i < num_trxs; ++i) {
    ok &= write(strm, trxs_[i]);
  }
  ok &= write(strm, sig_);
  ok &= write(strm, hash_);
  ok &= write(strm, cached_sender_);
  assert(ok);
  return ok;
}

bool DagBlock::deserialize(stream &strm) {
  uint32_t num_tips, num_trxs;
  bool ok = true;

  ok &= read(strm, num_tips);
  ok &= read(strm, num_trxs);
  ok &= read(strm, pivot_);
  ok &= read(strm, level_);

  for (auto i = 0; i < num_tips; ++i) {
    blk_hash_t t;
    ok &= read(strm, t);
    if (ok) {
      tips_.push_back(t);
    }
  }
  for (auto i = 0; i < num_trxs; ++i) {
    trx_hash_t t;
    ok &= read(strm, t);
    if (ok) {
      trxs_.push_back(t);
    }
  }

  ok &= read(strm, sig_);
  ok &= read(strm, hash_);
  ok &= read(strm, cached_sender_);
  assert(ok);
  return ok;
}

void DagBlock::sign(secret_t const &sk) {
  if (!sig_) {
    sig_ = dev::sign(sk, sha3(false));
  }
  sender();
  updateHash();
}

bool DagBlock::verifySig() const {
  if (!sig_) return false;
  auto msg = sha3(false);
  auto pk = dev::recover(sig_, msg);
  return dev::verify(pk, sig_, msg);
}

addr_t DagBlock::sender() const {
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

void DagBlock::streamRLP(dev::RLPStream &s, bool include_sig) const {
  auto num_tips = tips_.size();
  auto num_trxs = trxs_.size();
  auto total = 2 + num_tips + num_trxs;
  s.appendList(include_sig ? total + 1 : total);
  s << pivot_;
  s << level_;
  for (auto i = 0; i < num_tips; ++i) s << tips_[i];
  for (auto i = 0; i < num_trxs; ++i) s << trxs_[i];
  if (include_sig) {
    s << sig_;
  }
}

bytes DagBlock::rlp(bool include_sig) const {
  dev::RLPStream s;
  streamRLP(s, include_sig);
  return s.out();
}

blk_hash_t DagBlock::sha3(bool include_sig) const {
  return dev::sha3(rlp(include_sig));
}

BlockManager::BlockManager(size_t capacity, unsigned num_verifiers)
    : capacity_(capacity), num_verifiers_(num_verifiers) {}
BlockManager::~BlockManager() {
  if (!stopped_) {
    stop();
  }
}
void BlockManager::start() {
  if (!stopped_) return;
  LOG(log_nf_) << "Create verifier threads = " << num_verifiers_ << std::endl;
  if (!node_.lock()) {
    LOG(log_er_) << "FullNode is not set ...";
    return;
  }
  trx_mgr_ = node_.lock()->getTransactionManager();
  stopped_ = false;
  for (auto i = 0; i < num_verifiers_; ++i) {
    verifiers_.emplace_back([this, i]() { this->verifyBlock(); });
  }
}
void BlockManager::stop() {
  if (stopped_) return;
  { stopped_ = true; }
  cond_for_unverified_qu_.notify_all();
  cond_for_verified_qu_.notify_all();
  for (auto &t : verifiers_) {
    t.join();
  }
}

bool BlockManager::isBlockKnown(blk_hash_t const &hash) {
  boost::shared_lock<boost::shared_mutex> lock(shared_mutex_);
  return seen_blocks_.count(hash);
}

std::shared_ptr<DagBlock> BlockManager::getDagBlock(blk_hash_t const &hash) {
  boost::shared_lock<boost::shared_mutex> lock(shared_mutex_);
  std::shared_ptr<DagBlock> ret;
  auto blk = seen_blocks_.find(hash);
  if (blk != seen_blocks_.end()) {
    ret = std::make_shared<DagBlock>(blk->second);
  }
  return ret;
}

void BlockManager::pushUnverifiedBlock(
    DagBlock const &blk, std::vector<Transaction> const &transactions,
    bool critical) {
  {
    upgradableLock lock(shared_mutex_);
    if (seen_blocks_.count(blk.getHash())) {
      LOG(log_dg_) << "Seen block: " << blk.getHash() << std::endl;
      return;
    }

    upgradeLock locked(lock);
    seen_blocks_[blk.getHash()] = blk;
  }
  {
    uLock lock(shared_mutex_for_unverified_qu_);
    if (critical) {
      blk_status_.insert(blk.getHash(), BlockStatus::proposed);

      unverified_qu_.emplace_front(std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from front: " << blk.getHash()
                   << std::endl;
    } else {
      blk_status_.insert(blk.getHash(), BlockStatus::broadcasted);
      unverified_qu_.emplace_back(std::make_pair(blk, transactions));
      LOG(log_dg_) << "Insert unverified block from back: " << blk.getHash()
                   << std::endl;
    }
  }
  cond_for_unverified_qu_.notify_one();
}

void BlockManager::pushUnverifiedBlock(DagBlock const &blk, bool critical) {
  pushUnverifiedBlock(blk, std::vector<Transaction>(), critical);
}

DagBlock BlockManager::popVerifiedBlock() {
  uLock lock(shared_mutex_for_verified_qu_);
  while (verified_qu_.empty() && !stopped_) {
    cond_for_verified_qu_.wait(lock);
  }
  if (stopped_) return DagBlock();

  auto blk = verified_qu_.front().first;

  verified_qu_.pop_front();
  return blk;
}

void BlockManager::verifyBlock() {
  auto log_time = node_.lock()->getTimeLogger();
  while (!stopped_) {
    std::pair<DagBlock, std::vector<Transaction>> blk;
    {
      uLock lock(shared_mutex_for_unverified_qu_);
      while (unverified_qu_.empty() && !stopped_) {
        cond_for_unverified_qu_.wait(lock);
      }
      if (stopped_) return;
      blk = unverified_qu_.front();
      unverified_qu_.pop_front();
    }
    auto status = blk_status_.get(blk.first.getHash()).first;
    // Verifying transaction ...
    LOG(log_time) << "VerifyingTrx block  " << blk.first.getHash()
                  << " at: " << getCurrentTimeMilliSeconds();
    // only need to verify if this is a broadcasted block (proposed block are
    // generated by verified trx)
    if (status == BlockStatus::broadcasted) {
      bool valid = trx_mgr_->verifyBlockTransactions(blk.first, blk.second);
      if (!valid) {
        LOG(log_er_) << "Ignore block " << blk.first.getHash()
                     << " since it has invalid transactions";
        blk_status_.update(blk.first.getHash(), BlockStatus::invalid);
        continue;
      }
    }

    {
      uLock lock(shared_mutex_for_verified_qu_);
      if (status == BlockStatus::proposed) {
        verified_qu_.emplace_front(blk);
      } else if (status == BlockStatus::broadcasted) {
        verified_qu_.emplace_back(blk);
      }
    }
    blk_status_.update(blk.first.getHash(), BlockStatus::verified);

    LOG(log_time) << "VerifiedTrx stored " << blk.first.getHash()
                  << " at: " << getCurrentTimeMilliSeconds();

    cond_for_verified_qu_.notify_one();
    LOG(log_dg_) << "Verified block: " << blk.first.getHash() << std::endl;
  }
}  // namespace taraxa

}  // namespace taraxa
