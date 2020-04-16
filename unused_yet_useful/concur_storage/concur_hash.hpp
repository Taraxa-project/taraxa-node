#ifndef TARAXA_NODE_CONCUR_STORAGE_CONCUR_HASH_HPP
#define TARAXA_NODE_CONCUR_STORAGE_CONCUR_HASH_HPP
#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <vector>
#include "util.hpp"

namespace taraxa {
namespace storage {
// transaction type
using mem_addr_t = std::string;
using trx_t = std::string;

enum class ConflictStatus : uint8_t { read = 0, shared = 1, write = 2 };

class ConflictKey {
 public:
  ConflictKey(mem_addr_t const &contract, mem_addr_t const &storage)
      : contract_(contract), storage_(storage) {}
  std::size_t getHash() const {
    return std::hash<std::string>()(contract_ + storage_);
  }
  mem_addr_t getContract() const { return contract_; }
  mem_addr_t getStorage() const { return storage_; }
  bool operator<(ConflictKey const &other) const {
    return getHash() < other.getHash();
  }
  bool operator==(ConflictKey const &other) const {
    return getHash() == other.getHash();
  }
  friend std::ostream &operator<<(std::ostream &strm, ConflictKey const &key) {
    strm << "ConflictKey: " << key.contract_ << " " << key.storage_
         << std::endl;
    return strm;
  }

 private:
  mem_addr_t contract_;
  mem_addr_t storage_;
};

class ConflictValue {
 public:
  ConflictValue() : trx_(std::string()), status_(ConflictStatus::read) {}
  ConflictValue(trx_t const &trx, ConflictStatus status)
      : trx_(trx), status_(status) {}
  trx_t getTrx() { return trx_; }
  ConflictStatus getStatus() { return status_; }
  bool operator==(ConflictValue const &other) const {
    return trx_ == other.trx_ && status_ == other.status_;
  }

  friend std::ostream &operator<<(std::ostream &strm,
                                  ConflictValue const &value) {
    strm << "ConflictValue: " << value.trx_ << " " << asInteger(value.status_)
         << std::endl;
    return strm;
  }

 private:
  trx_t trx_;
  ConflictStatus status_;
};

/**
 * concur_degree is exponent, determines the granularity of locks,
 * cannot change once ConcurHahs is constructed
 * number of locks = 2^(concur_degree)
 *
 * Note: Do resize up but not down for now
 * Note: the hash content is expected to be modified through insertOrGet
 */

template <typename Key, typename Value>
class ConcurHash {
 public:
  /** An Item should define
   * key, value
   * getHash()
   * operator ==
   */

  struct Item {
    Item(Key const &key, Value const &value) : key(key), value(value) {}
    bool operator<(Item const &other) const {
      return key.getHash() < other.key.getHash();
    }
    bool operator==(Item const &other) const {
      return key.getHash() == other.key.getHash();
    }
    Key key;
    Value value;
  };

  using ulock = std::unique_lock<std::mutex>;
  using bucket = std::list<Item>;
  ConcurHash(unsigned concur_degree_exp);
  // return true if key not exist
  bool insert(Key key, Value value);
  // bool return false if not existence
  std::pair<Value, bool> get(Key key);
  // return true if:
  // 1. key exist and swap success
  // 2. key not exist, expected_value is dummy, then insert
  bool compareAndSwap(Key key, Value expected_value, Value new_value);
  bool remove(Key key);
  void clear();

  void setVerbose(bool verbose) { verbose_ = verbose; }
  std::size_t getConcurrentDegree();
  // query only if no threads is writing ...
  std::size_t getBucketSize();
  uint64_t getItemSize();

 private:
  unsigned getBucketId(Key key) const;
  unsigned getLockId(Key key) const;
  void resize();        // double buckets_ size
  bool policy() const;  // determines when to do resize
  bool verbose_;
  const std::size_t concur_degree_;
  const std::size_t init_bucket_size_;
  std::atomic<uint64_t> total_size_;  // total number of items
  std::atomic<std::size_t> bucket_size_;
  std::vector<bucket> buckets_;
  std::vector<std::mutex> mutexes_;
};

using ConflictHash = ConcurHash<ConflictKey, ConflictValue>;

}  // namespace storage
}  // namespace taraxa

#endif
