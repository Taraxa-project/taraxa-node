#include "database_face_cache.hpp"
#include <memory>
#include <utility>
#include "util/eth.hpp"

std::string DatabaseFaceCache::lookup(dev::db::Slice _key) const {
  auto value = memory_cache_.get(_key.toString());
  if (!value.second) {
    return db_->lookup(_key);
  }
  return value.first.toString();
}

bool DatabaseFaceCache::exists(dev::db::Slice _key) const {
  if (!memory_cache_.count(_key.toString())) {
    return db_->exists(_key);
  }
  return true;
}

void DatabaseFaceCache::insert(dev::db::Slice _key, dev::db::Slice _value) {
  memory_cache_.update(_key.toString(), _value);
  db_->insert(_key, _value);
}

std::unique_ptr<dev::db::WriteBatchFace> DatabaseFaceCache::createWriteBatch() {
  return db_->createWriteBatch();
}

void DatabaseFaceCache::commit(std::unique_ptr<dev::db::WriteBatchFace> _batch) {
  db_->commit(_batch);
}

void DatabaseFaceCache::forEach(
      std::function<bool(dev::db::Slice, dev::db::Slice)> f) const {
        db_->forEach(f);
      }
