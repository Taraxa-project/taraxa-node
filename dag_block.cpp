/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-10-31 16:26:04
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-14 17:50:19
 */
#include "dag_block.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <utility>
#include "dag.hpp"

namespace taraxa {

using std::to_string;

blk_hash_t DagBlock::getPivot() const { return pivot_; }
vec_tip_t DagBlock::getTips() const { return tips_; }
vec_trx_t DagBlock::getTrxs() const { return trxs_; }
sig_t DagBlock::getSignature() const { return signature_; }
blk_hash_t DagBlock::getHash() const { return hash_; }
name_t DagBlock::getPublisher() const { return publisher_; }
DagBlock::DagBlock(blk_hash_t pivot, vec_tip_t tips, vec_trx_t trxs,
                   sig_t signature, blk_hash_t hash, name_t publisher) try
    : pivot_(pivot),
      tips_(tips),
      trxs_(trxs),
      signature_(signature),
      hash_(hash),
      publisher_(publisher) {
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
}
DagBlock::DagBlock(DagBlock &&blk)
    : pivot_(std::move(blk.pivot_)),
      tips_(std::move(blk.tips_)),
      trxs_(std::move(blk.trxs_)),
      signature_(std::move(blk.signature_)),
      hash_(std::move(blk.hash_)),
      publisher_(std::move(blk.publisher_)) {}

DagBlock::DagBlock(stream &strm) { deserialize(strm); }
DagBlock::DagBlock(std::string const &json) {
  try {
    boost::property_tree::ptree doc = strToJson(json);
    pivot_ = blk_hash_t(doc.get<std::string>("pivot"));
    tips_ = asVector<blk_hash_t, std::string>(doc, "tips");
    trxs_ = asVector<trx_hash_t, std::string>(doc, "trxs");
    signature_ = sig_t(doc.get<std::string>("sig"));
    hash_ = blk_hash_t(doc.get<std::string>("hash"));
    publisher_ = blk_hash_t(doc.get<std::string>("pub"));
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
bool DagBlock::isValid() const {
  return !(pivot_.isZero() && hash_.isZero() && signature_.isZero() &&
           publisher_.isZero());
}

std::string DagBlock::getJsonStr() const {
  using boost::property_tree::ptree;

  ptree tree;
  tree.put("pivot", pivot_.toString());

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

  tree.put("sig", signature_.toString());
  tree.put("hash", hash_.toString());
  tree.put("pub", publisher_.toString());

  std::stringstream ostrm;
  boost::property_tree::write_json(ostrm, tree);
  return ostrm.str();
}

bool DagBlock::serialize(stream &strm) const {
  bool ok = true;
  uint8_t num_tips = tips_.size();
  uint8_t num_trxs = trxs_.size();
  ok &= write(strm, num_tips);
  ok &= write(strm, num_trxs);
  ok &= write(strm, pivot_);
  for (auto i = 0; i < num_tips; ++i) {
    ok &= write(strm, tips_[i]);
  }
  for (auto i = 0; i < num_trxs; ++i) {
    ok &= write(strm, trxs_[i]);
  }
  ok &= write(strm, signature_);
  ok &= write(strm, hash_);
  ok &= write(strm, publisher_);
  assert(ok);
  return ok;
}

bool DagBlock::deserialize(stream &strm) {
  uint8_t num_tips, num_trxs;
  bool ok = true;

  ok &= read(strm, num_tips);
  ok &= read(strm, num_trxs);
  ok &= read(strm, pivot_);
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
  ok &= read(strm, signature_);
  ok &= read(strm, hash_);
  ok &= read(strm, publisher_);
  assert(ok);
  return ok;
}

bool DagBlock::operator==(DagBlock const &other) const {
  return this->getJsonStr() == other.getJsonStr();
}
DagBlock &DagBlock::operator=(DagBlock &&other) {
  pivot_ = std::move(other.pivot_);
  tips_ = std::move(other.tips_);
  trxs_ = std::move(other.trxs_);
  signature_ = std::move(other.signature_);
  hash_ = std::move(other.hash_);
  publisher_ = std::move(other.publisher_);
  return *this;
}
BlockQueue::BlockQueue(size_t capacity, unsigned num_verifiers)
    : capacity_(capacity), num_verifiers_(num_verifiers) {}
BlockQueue::~BlockQueue() {
  if (!stopped_) {
    stop();
  }
}
void BlockQueue::start() {
  if (!stopped_) return;
  stopped_ = false;
  for (auto i = 0; i < num_verifiers_; ++i) {
    verifiers_.emplace_back([this, i]() { this->verifyBlock(); });
  }
}
void BlockQueue::stop() {
  if (stopped_) return;
  {
    uLock lock(mutex_);
    stopped_ = true;
  }
  cond_for_unverified_qu_.notify_all();
  cond_for_verified_qu_.notify_all();
  for (auto &t : verifiers_) {
    t.join();
  }
}

bool BlockQueue::isBlockKnown(blk_hash_t const &hash) {
  boost::shared_lock<boost::shared_mutex> lock(shared_mutex_);
  return seen_blocks_.count(hash);
}

std::shared_ptr<DagBlock> BlockQueue::getBlock(blk_hash_t const &hash) {
  boost::shared_lock<boost::shared_mutex> lock(shared_mutex_);
  auto fBlk = seen_blocks_.find(hash);
  if(fBlk != seen_blocks_.end())
    return std::make_shared<DagBlock>(fBlk->second);  
  return std::shared_ptr<DagBlock>();
}

void BlockQueue::pushUnverifiedBlock(DagBlock const &blk) {
  {
    upgradableLock lock(shared_mutex_);
    if (seen_blocks_.count(blk.getHash())) {
      LOG(logger_) << "Seen block: " << blk.getHash() << std::endl;
      return;
    }

    LOG(logger_) << "Insert block: " << blk.getHash() << std::endl;
    upgradeLock locked(lock);
    seen_blocks_[blk.getHash()] = blk;
  }
  {
    uLock lock(mutex_for_unverified_qu_);
    unverified_qu_.emplace_back((blk));
    cond_for_unverified_qu_.notify_one();
  }
}

DagBlock BlockQueue::getVerifiedBlock() {
  uLock lock(mutex_for_verified_qu_);
  while (verified_qu_.empty() && !stopped_) {
    cond_for_verified_qu_.wait(lock);
  }
  DagBlock blk;
  if (stopped_) return blk;

  blk = verified_qu_.front();

  verified_qu_.pop_front();
  return blk;
}

void BlockQueue::verifyBlock() {
  while (!stopped_) {
    DagBlock blk;
    {
      uLock lock(mutex_for_unverified_qu_);
      while (unverified_qu_.empty() && !stopped_) {
        cond_for_unverified_qu_.wait(lock);
      }
      if (stopped_) return;
      blk = unverified_qu_.front();
      unverified_qu_.pop_front();
    }

    // TODO: verify block, now just move it to verified_qu_
    {
      uLock lock(mutex_for_verified_qu_);
      verified_qu_.emplace_back(blk);
      cond_for_verified_qu_.notify_one();
    }
  }
}

}  // namespace taraxa
