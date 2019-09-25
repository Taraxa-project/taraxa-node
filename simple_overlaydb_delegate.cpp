#include "simple_overlaydb_delegate.hpp"

bool SimpleOverlayDBDelegate::put(const std::string &key,
                                  const std::string &value) {
  bool succ = string_cache_.insert(key, value);
  if (succ) {
    const h256 hashKey = stringToHashKey(key);
    upgradableLock lock(shared_mutex_);
    if (odb_->exists(hashKey)) {
      return false;
    }
    upgradeLock locked(lock);
    odb_->insert(hashKey, value);
    return true;
  }
  return false;
}
bool SimpleOverlayDBDelegate::update(const std::string &key,
                                     const std::string &value) {
  string_cache_.update(key, value);
  boost::unique_lock lock(shared_mutex_);
  odb_->insert(stringToHashKey(key), value);
  return true;
}
std::string SimpleOverlayDBDelegate::get(const std::string &key) {
  auto value = string_cache_.get(key);
  if (!value.second) {
    const h256 hashKey = stringToHashKey(key);
    sharedLock lock(shared_mutex_);
    return odb_->lookup(hashKey);
  }
  return value.first;
}
bool SimpleOverlayDBDelegate::put(const h256 &key, const dev::bytes &value) {
  bool succ = binary_cache_.insert(key, value);
  if (succ) {
    upgradableLock lock(shared_mutex_);
    if (odb_->existsAux(key)) {
      return false;
    }
    upgradeLock locked(lock);
    odb_->insertAux(key, &value);
    return true;
  }
  return false;
}
bool SimpleOverlayDBDelegate::update(const h256 &key, const dev::bytes &value) {
  binary_cache_.update(key, value);
  boost::unique_lock lock(shared_mutex_);
  odb_->insertAux(key, &value);
  return true;
}
dev::bytes SimpleOverlayDBDelegate::get(const h256 &key) {
  auto value = binary_cache_.get(key);
  if (!value.second) {
    sharedLock lock(shared_mutex_);
    return odb_->lookupAux(key);
  }
  return value.first;
}
bool SimpleOverlayDBDelegate::exists(const h256 &key) {
  if (!binary_cache_.count(key)) {
    sharedLock lock(shared_mutex_);
    return odb_->existsAux(key);
  }
  return true;
}
void SimpleOverlayDBDelegate::commit() {
  boost::unique_lock lock(shared_mutex_);
  odb_->commit();
}

SimpleOverlayDBDelegate::SimpleOverlayDBDelegate(const std::string &path,
                                                 bool overwrite,
                                                 uint32_t binary_cache_size,
                                                 uint32_t string_cache_size)
    : odb_(std::make_shared<dev::OverlayDB>(dev::OverlayDB(
          dev::eth::State::openDB(path, TEMP_GENESIS_HASH,
                                  overwrite ? dev::WithExisting::Kill
                                            : dev::WithExisting::Trust)))),
      binary_cache_size_(binary_cache_size),
      string_cache_size_(string_cache_size),
      binary_cache_(binary_cache_size, binary_cache_size / 100),
      string_cache_(string_cache_size, string_cache_size / 100) {}