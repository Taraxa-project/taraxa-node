#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace taraxa {

// TODO: could it be somehow prettier?
// partial specialization is not allowed for functions, so make it with structs
namespace {
template <class T>
struct is_empty {
  is_empty(const T &v) : v_(v) {}
  operator bool() const { return !v_; }
  const T &v_;
};
template <class T>
struct is_empty<std::vector<T>> {
  is_empty(const std::vector<T> &v) : v_(v) {}
  operator bool() const { return v_.empty(); }
  const std::vector<T> &v_;
};
}  // namespace

template <class Key, class Value>
class MapByBlockCache {
 public:
  using GetterFn = std::function<Value(uint64_t, const Key &)>;
  using ValueMap = std::unordered_map<Key, Value>;
  using DataMap = std::map<uint64_t, ValueMap>;

  MapByBlockCache(const MapByBlockCache &) = delete;
  MapByBlockCache(MapByBlockCache &&) = delete;
  MapByBlockCache &operator=(const MapByBlockCache &) = delete;
  MapByBlockCache &operator=(MapByBlockCache &&) = delete;

  MapByBlockCache(uint64_t blocks_to_save, GetterFn getter_fn) : kBlocksToKeep(blocks_to_save), getter_fn_(getter_fn) {}

  void cleanup() {
    if (data_by_block_.size() > kBlocksToKeep) {
      data_by_block_.erase(data_by_block_.begin());
    }
  }

  typename DataMap::iterator newBlockEntry(uint64_t block_num) const {
    auto emplace_res = data_by_block_.emplace(block_num, ValueMap());

    // Remove older element after we added one more
    if (data_by_block_.size() > kBlocksToKeep) {
      data_by_block_.erase(data_by_block_.begin());
    }

    return emplace_res.first;
  }

  Value get(uint64_t blk_num, const Key &key) const {
    auto blk_entry = data_by_block_.find(blk_num);
    if (blk_entry != data_by_block_.end()) {
      std::shared_lock lock(mutex_);
      auto e = blk_entry->second.find(key);
      if (e != blk_entry->second.end()) {
        return e->second;
      }
    }

    auto value = getter_fn_(blk_num, key);
    if (is_empty(value)) {
      return {};
    }
    // Not save old values in cache
    auto last_num = lastBlockNum();
    if (last_num < kBlocksToKeep || blk_num >= last_num - kBlocksToKeep) {
      std::unique_lock lock(mutex_);
      auto blk_entry = newBlockEntry(blk_num);
      blk_entry->second.emplace(key, value);
    }
    return value;
  }

  uint64_t lastBlockNum() const {
    std::shared_lock lock(mutex_);
    if (data_by_block_.empty()) {
      return 0;
    }
    return data_by_block_.rbegin()->first;
  }

 protected:
  const uint64_t kBlocksToKeep;
  GetterFn getter_fn_;

  // cache is used from const methods in other class, so should be mutable
  mutable std::shared_mutex mutex_;
  mutable DataMap data_by_block_;
};

template <class Value>
class ValueByBlockCache {
 public:
  using GetterFn = std::function<Value(uint64_t)>;
  using DataMap = std::map<uint64_t, Value>;

  ValueByBlockCache(const ValueByBlockCache &) = delete;
  ValueByBlockCache(ValueByBlockCache &&) = delete;
  ValueByBlockCache &operator=(const ValueByBlockCache &) = delete;
  ValueByBlockCache &operator=(ValueByBlockCache &&) = delete;

  ValueByBlockCache(uint64_t blocks_to_save, GetterFn getter_fn)
      : kBlocksToKeep(blocks_to_save), getter_fn_(getter_fn) {}

  void append(uint64_t block_num, Value value) const {
    std::unique_lock lock(mutex_);
    data_by_block_.emplace(block_num, value);

    // Remove older element after we added one more
    if (data_by_block_.size() > kBlocksToKeep) {
      data_by_block_.erase(data_by_block_.begin());
    }
  }

  Value get(uint64_t block_num) const {
    {
      std::shared_lock lock(mutex_);
      auto blk_entry = data_by_block_.find(block_num);
      if (blk_entry != data_by_block_.end()) {
        return blk_entry->second;
      }
    }

    auto value = getter_fn_(block_num);
    if (is_empty(value)) {
      return {};
    }
    // Not save old values in cache
    auto last_num = lastBlockNum();
    if (last_num < kBlocksToKeep || block_num >= last_num - kBlocksToKeep) {
      append(block_num, value);
    }
    return value;
  }

  Value last() const {
    std::shared_lock lock(mutex_);
    if (data_by_block_.empty()) {
      return {};
    }
    return data_by_block_.rbegin()->second;
  }

  uint64_t lastBlockNum() const {
    std::shared_lock lock(mutex_);
    if (data_by_block_.empty()) {
      return 0;
    }
    return data_by_block_.rbegin()->first;
  }

 protected:
  const uint64_t kBlocksToKeep;
  GetterFn getter_fn_;

  // cache is used from const methods in other class, so should be mutable
  mutable std::shared_mutex mutex_;
  mutable DataMap data_by_block_;
};

}  // namespace taraxa