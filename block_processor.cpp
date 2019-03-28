#include "block_processor.hpp"
#include "dag_block.hpp"

namespace taraxa {
BlockProcessor::BlockProcessor(std::weak_ptr<FullNode> node) : node_(node) {}

BlockProcessor::~BlockProcessor() { stop(); }
void BlockProcessor::stop() {
  std::unique_lock<std::mutex> lock(mutex_);
  stopped_ = true;
  condition_.notify_all();
}
void BlockProcessor::add(std::shared_ptr<DagBlock> sp, time_t time) {
  // TODO: test work validation

  std::unique_lock<std::mutex> lock(mutex_);

  // QQ: No test for full???
  if (block_hashes_.find(sp->getHash()) == block_hashes_.end()) {
    dag_blocks_.push_back({sp, time});

    // QQ: Where is block_hashes inserted???
    block_hashes_.insert(sp->getHash());
  }
  // QQ: notify_one() sufficient?
  condition_.notify_all();
}

bool BlockProcessor::full() {
  std::unique_lock<std::mutex> lock(mutex_);
  return dag_blocks_.size() > MAX_STATE_BLOCKS;
}

void BlockProcessor::processBlocks() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (!stopped_) {
    if (haveBlocks()) {
      active_ = true;
      lock.unlock();
      processManyBlocks(lock);
      lock.lock();
      active_ = false;
      // QQ: ???
    } else {
      condition_.notify_all();  // QQ: notify who ??
      condition_.wait(lock);
    }
  }
}

// see above, mutex must be locked before checking haveBlocks
// TODO: change it, strange

bool BlockProcessor::haveBlocks() {
  assert(!mutex_.try_lock());
  bool ret = !dag_blocks_.empty();
  return ret;
}

void BlockProcessor::batchVerifyDagBlocks(std::unique_lock<std::mutex> &lock) {
  // TODO:
  // now assume all block valid
  lock.lock();
  for (auto &s : dag_blocks_) {
    verified_blocks_.emplace_back(s);
  }
  dag_blocks_.clear();
  lock.unlock();

  /*
  // copy entire deque
  lock.lock();
  std::deque<std::pair<std::shared_ptr<taraxa::DagBlocks>, time_t >> tmp;
  tmp.swap(dag_blocks_);
  lock.unlock();

  // batch verify
  size_t sz = tmp.size();
  std::vector<taraxa::block_hash_t> hashes;
  std::vector<unsigned char const *> messages;
  std::vector<size_t> lengths;
  std::vector<unsigned char const *> pks;
  std::vector<unsigned char const *> signatures;
  std::vector<int> verifications;
  hashes.reserve(sz);
  messages.reserve(sz);
  lengths.reserve(sz);
  pks.reserve(sz);
  signatures.reserve(sz);
  verifications.reserve(sz);
  for (auto i=0; i<sz; ++i){
          // TODO: ... batch verify
  }

*/
}

void BlockProcessor::processManyBlocks(std::unique_lock<std::mutex> &lock) {
  // one of the thread has to do the batch verify stuff
  batchVerifyDagBlocks(lock);
  auto start_time(std::chrono::steady_clock::now());
  lock.lock();
  while (!verified_blocks_.empty()) {
    std::pair<std::shared_ptr<taraxa::DagBlock>, time_t> block;
    block = verified_blocks_.front();
    verified_blocks_.pop_front();
    auto hash = block.first->getHash();
    block_hashes_.erase(hash);
    lock.unlock();
    // multi-threaded
    processOneBlock(block.first, block.second, true);
    lock.lock();
  }
  lock.unlock();
}

ProcessReturn BlockProcessor::processOneBlock(
    std::shared_ptr<taraxa::DagBlock> sb, time_t origination, bool validated) {
  ProcessReturn result;
  auto hash(sb->getHash());
  // TODO: ledger process
  return result;
}

}  // namespace taraxa
