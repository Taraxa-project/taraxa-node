#ifndef TARAXA_NODE_DATABASE_FACE_CACHE_HPP
#define TARAXA_NODE_DATABASE_FACE_CACHE_HPP

#include "libdevcore/db.h"
#include "util.hpp"

class DatabaseFaceCache {
 public:
  DatabaseFaceCache(std::unique_ptr<dev::db::DatabaseFace> db,
                    uint32_t memory_cache_size = 1000)
      : db_(std::move(db)), memory_cache_size_(memory_cache_size), memory_cache_(memory_cache_size, memory_cache_size / 100) {}

  virtual std::string lookup(dev::db::Slice _key) const;
  virtual bool exists(dev::db::Slice _key) const;
  virtual void insert(dev::db::Slice _key, dev::db::Slice _value);
  
  inline dev::db::Slice toSlice(dev::bytes const& _b) const
  {
      return dev::db::Slice(reinterpret_cast<char const*>(&_b[0]), _b.size());
  }

  virtual dev::bytes lookup(dev::h256 _key) const {
    return dev::asBytes(lookup(toSlice(_key.asBytes())));
  }

  virtual bool exists(dev::h256 _key) const {
    return exists(toSlice(_key.asBytes()));
  }

  virtual void insert(dev::h256 _key, dev::bytes _value) {
    insert(toSlice(_key.asBytes()), toSlice(_value));
  }
  
  virtual std::unique_ptr<dev::db::WriteBatchFace> createWriteBatch();
  virtual void commit(std::unique_ptr<dev::db::WriteBatchFace> _batch);

  // A database must implement the `forEach` method that allows the caller
  // to pass in a function `f`, which will be called with the key and value
  // of each record in the database. If `f` returns false, the `forEach`
  // method must return immediately.
  virtual void forEach(
      std::function<bool(dev::db::Slice, dev::db::Slice)> f) const;

 private:
  std::unique_ptr<dev::db::DatabaseFace> db_;
  ExpirationCacheMap<std::string, dev::db::Slice> memory_cache_;
  uint32_t memory_cache_size_;
};
#endif  // TARAXA_NODE_SIMPLE_OVERLAYDB_DELEGATE_HPP
