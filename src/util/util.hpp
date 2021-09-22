#pragma once

#include <execinfo.h>
#include <json/json.h>
#include <libdevcore/RLP.h>
#include <signal.h>

#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <iostream>
#include <list>
#include <optional>
#include <regex>
#include <streambuf>
#include <string>
#include <unordered_set>

#include "common/types.hpp"

namespace taraxa {

template <typename T>
std::weak_ptr<T> as_weak(std::shared_ptr<T> sp) {
  return std::weak_ptr<T>(sp);
}

template <typename Int1, typename Int2>
auto int_pow(Int1 x, Int2 y) {
  if (!y) {
    return 1;
  }
  while (--y > 0) {
    x *= x;
  }
  return x;
}

template <typename T, typename U = T>
std::vector<T> asVector(Json::Value const &json, std::string const &key) {
  std::vector<T> v;
  auto key_child = json[key];
  std::transform(key_child.begin(), key_child.end(), std::back_inserter(v),
                 [](const Json::Value &item) { return T(item.asString()); });
  return v;
}

template <typename T>
std::vector<T> asVector(Json::Value const &json) {
  std::vector<T> v;
  std::transform(json.begin(), json.end(), std::back_inserter(v),
                 [](const Json::Value &item) { return T(item.asString()); });
  return v;
}

template <typename T>
std::ostream &operator<<(std::ostream &strm, std::vector<T> const &vec) {
  for (auto const &i : vec) {
    strm << i << " ";
  }
  return strm;
}
template <typename U, typename V>
std::ostream &operator<<(std::ostream &strm, std::pair<U, V> const &pair) {
  strm << "[ " << pair.first << " , " << pair.second << "]";
  return strm;
}

using stream = std::basic_streambuf<uint8_t>;
using bufferstream = boost::iostreams::stream_buffer<boost::iostreams::basic_array_source<uint8_t>>;
using vectorstream = boost::iostreams::stream_buffer<boost::iostreams::back_insert_device<std::vector<uint8_t>>>;

// Read a raw byte stream the size of T and fill value
// return true if success
template <typename T>
bool read(stream &stm, T &value) {
  static_assert(std::is_standard_layout<T>::value, "Cannot stream read non-standard layout types");
  auto bytes(stm.sgetn(reinterpret_cast<uint8_t *>(&value), sizeof(value)));
  return bytes == sizeof(value);
}

template <typename T>
bool write(stream &stm, T const &value) {
  static_assert(std::is_standard_layout<T>::value, "Cannot stream write non-standard layout types");
  auto bytes(stm.sputn(reinterpret_cast<uint8_t const *>(&value), sizeof(value)));
  assert(bytes == sizeof(value));
  return bytes == sizeof(value);
}

void thisThreadSleepForSeconds(unsigned sec);
void thisThreadSleepForMilliSeconds(unsigned millisec);
void thisThreadSleepForMicroSeconds(unsigned microsec);

unsigned long getCurrentTimeMilliSeconds();

std::string getFormattedVersion(std::initializer_list<uint32_t> list);

/**
 * simple thread_safe hash
 * LRU
 */

template <typename K, typename V>
class StatusTable {
 public:
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;
  using UnsafeStatusTable = std::unordered_map<K, V>;
  StatusTable(size_t capacity = 10000) : capacity_(capacity) {}
  std::pair<V, bool> get(K const &hash) {
    uLock lock(shared_mutex_);
    auto iter = status_.find(hash);
    if (iter != status_.end()) {
      auto kv = *(iter->second);
      auto &k = kv.first;
      auto &v = kv.second;
      lru_.erase(iter->second);
      lru_.push_front(kv);
      status_[k] = lru_.begin();
      return {v, true};
    } else {
      return {V(), false};
    }
  }
  unsigned long size() const {
    sharedLock lock(shared_mutex_);
    return status_.size();
  }
  bool insert(K const &hash, V status) {
    uLock lock(shared_mutex_);
    bool ret = false;
    if (status_.count(hash)) {
      ret = false;
    } else {
      if (status_.size() == capacity_) {
        clearOldData();
      }
      lru_.push_front({hash, status});
      status_[hash] = lru_.begin();
      ret = true;
    }
    assert(status_.size() == lru_.size());
    return ret;
  }
  void update(K const &hash, V status) {
    uLock lock(shared_mutex_);
    auto iter = status_.find(hash);
    if (iter != status_.end()) {
      lru_.erase(iter->second);
      lru_.push_front({hash, status});
      status_[hash] = lru_.begin();
    }
    assert(status_.size() == lru_.size());
  }

  bool update(K const &hash, V status, V expected_status) {
    bool ret = false;
    uLock lock(shared_mutex_);
    auto iter = status_.find(hash);
    if (iter != status_.end() && iter->second == expected_status) {
      lru_.erase(iter->second);
      lru_.push_front({hash, status});
      status_[hash] = lru_.begin();
      ret = true;
    }
    assert(status_.size() == lru_.size());
    return ret;
  }
  // clear everything
  void clear() {
    uLock lock(shared_mutex_);
    status_.clear();
    lru_.clear();
  }
  bool erase(K const &hash) {
    bool ret = false;
    uLock lock(shared_mutex_);
    auto iter = status_.find(hash);
    if (iter != status_.end()) {
      lru_.erase(iter->second);
      status_.erase(hash);
    }
    assert(status_.size() == lru_.size());
    return ret;
  }

  UnsafeStatusTable getThreadUnsafeCopy() const {
    sharedLock lock(shared_mutex_);
    return status_;
  }

 private:
  void clearOldData() {
    int sz = capacity_ / 10;
    for (int i = 0; i < sz; ++i) {
      auto kv = lru_.rbegin();
      auto &k = kv->first;
      status_.erase(k);
      lru_.pop_back();
      assert(!lru_.empty());  // should not happen
    }
  }
  using Element = std::list<std::pair<K, V>>;
  size_t capacity_;
  mutable boost::shared_mutex shared_mutex_;
  std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> status_;
  std::list<std::pair<K, V>> lru_;
};  // namespace taraxa

template <typename... TS>
std::string fmt(const std::string &pattern, const TS &...args) {
  return (boost::format(pattern) % ... % args).str();
}

}  // namespace taraxa

template <class Key>
class ExpirationCache {
 public:
  ExpirationCache(uint32_t max_size, uint32_t delete_step) : max_size_(max_size), delete_step_(delete_step) {}

  bool insert(Key const &key) {
    boost::unique_lock lck(mtx_);

    if (cache_.find(key) != cache_.end()) {
      return false;
    }

    cache_.insert(key);
    expiration_.push_back(key);
    if (cache_.size() > max_size_) {
      for (uint32_t i = 0; i < delete_step_; i++) {
        cache_.erase(expiration_.front());
        expiration_.pop_front();
      }
    }

    return true;
  }

  std::size_t count(Key const &key) const {
    boost::shared_lock lck(mtx_);
    return cache_.count(key);
  }

  void clear() {
    boost::unique_lock lck(mtx_);
    cache_.clear();
    expiration_.clear();
  }

 private:
  std::unordered_set<Key> cache_;
  std::deque<Key> expiration_;
  const uint32_t max_size_;
  const uint32_t delete_step_;
  mutable boost::shared_mutex mtx_;
};

template <typename T>
auto slice(std::vector<T> const &v, std::size_t from = -1, std::size_t to = -1) {
  auto b = v.begin();
  return std::vector<T>(from == (std::size_t)-1 ? b : b + from, to == (std::size_t)-1 ? v.end() : b + to);
}

template <typename T>
auto getRlpBytes(T const &t) {
  dev::RLPStream s;
  s << t;
  return s.out();
}

template <class Key, class Value>
class ExpirationCacheMap {
 public:
  ExpirationCacheMap(uint32_t max_size, uint32_t delete_step) : max_size_(max_size), delete_step_(delete_step) {}

  bool insert(Key const &key, Value const &value) {
    boost::unique_lock lck(mtx_);
    if (cache_.find(key) != cache_.end()) return false;

    cache_[key] = value;
    expiration_.push_back(key);
    if (cache_.size() > max_size_) {
      eraseOldest();
    }
    return true;
  }

  std::size_t count(Key const &key) const {
    boost::shared_lock lck(mtx_);
    return cache_.count(key);
  }

  std::size_t size() const {
    boost::shared_lock lck(mtx_);
    return cache_.size();
  }

  std::pair<Value, bool> get(Key const &key) const {
    boost::shared_lock lck(mtx_);
    auto it = cache_.find(key);
    if (it == cache_.end()) return std::make_pair(Value(), false);
    return std::make_pair(it->second, true);
  }

  void clear() {
    boost::unique_lock lck(mtx_);
    cache_.clear();
    expiration_.clear();
  }

  void update(Key const &key, Value value) {
    boost::unique_lock lck(mtx_);

    if (cache_.find(key) != cache_.end()) {
      expiration_.erase(std::remove(expiration_.begin(), expiration_.end(), key), expiration_.end());
    }

    cache_[key] = value;
    expiration_.push_back(key);

    if (cache_.size() > max_size_) {
      for (uint32_t i = 0; i < delete_step_; i++) {
        cache_.erase(expiration_.front());
        expiration_.pop_front();
      }
    }
  }

  void erase(const Key &key) {
    boost::unique_lock lck(mtx_);
    cache_.erase(key);
    expiration_.erase(std::remove(expiration_.begin(), expiration_.end(), key), expiration_.end());
  }

  std::pair<Value, bool> updateWithGet(Key const &key, Value value) {
    boost::unique_lock lck(mtx_);
    std::pair<Value, bool> ret;
    auto it = cache_.find(key);
    if (it == cache_.end()) {
      ret = std::make_pair(Value(), false);
    } else {
      ret = std::make_pair(it->second, true);
    }
    cache_[key] = value;
    expiration_.push_back(key);
    if (cache_.size() > max_size_) {
      for (uint32_t i = 0; i < delete_step_; i++) {
        cache_.erase(expiration_.front());
        expiration_.pop_front();
      }
    }
    return ret;
  }

  std::unordered_map<Key, Value> getRawMap() {
    boost::shared_lock lck(mtx_);
    return cache_;
  }

  bool update(Key const &key, Value value, Value expected_value) {
    boost::unique_lock lck(mtx_);
    auto it = cache_.find(key);
    if (it != cache_.end() && it->second == expected_value) {
      it->second = value;
      expiration_.push_back(key);
      if (cache_.size() > max_size_) {
        for (auto i = 0; i < delete_step_; i++) {
          cache_.erase(expiration_.front());
          expiration_.pop_front();
        }
      }
      return true;
    }
    return false;
  }

 protected:
  virtual void eraseOldest() {
    for (uint32_t i = 0; i < delete_step_; i++) {
      cache_.erase(expiration_.front());
      expiration_.pop_front();
    }
  }

  std::unordered_map<Key, Value> cache_;
  std::deque<Key> expiration_;
  const uint32_t max_size_;
  const uint32_t delete_step_;
  mutable boost::shared_mutex mtx_;
};
