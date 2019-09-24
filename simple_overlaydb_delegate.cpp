#include "simple_overlaydb_delegate.hpp"

bool SimpleOverlayDBDelegate::put(const std::string &key,
                                  const std::string &value) {
  const h256 hashKey = stringToHashKey(key);
  upgradableLock lock(shared_mutex_);
  if (odb_->exists(hashKey)) {
    return false;
  }
  upgradeLock locked(lock);
  odb_->insert(hashKey, value);
  return true;
}
bool SimpleOverlayDBDelegate::update(const std::string &key,
                                     const std::string &value) {
  boost::unique_lock lock(shared_mutex_);
  odb_->insert(stringToHashKey(key), value);
  return true;
}
std::string SimpleOverlayDBDelegate::get(const std::string &key) {
  const h256 hashKey = stringToHashKey(key);
  sharedLock lock(shared_mutex_);
  return odb_->lookup(hashKey);
}
bool SimpleOverlayDBDelegate::put(const h256 &key, const dev::bytes &value) {
  upgradableLock lock(shared_mutex_);
  if (odb_->existsAux(key)) {
    return false;
  }
  upgradeLock locked(lock);
  odb_->insertAux(key, &value);
  return true;
}
bool SimpleOverlayDBDelegate::update(const h256 &key, const dev::bytes &value) {
  boost::unique_lock lock(shared_mutex_);
  odb_->insertAux(key, &value);
  return true;
}
dev::bytes SimpleOverlayDBDelegate::get(const h256 &key) {
  sharedLock lock(shared_mutex_);
  auto value = odb_->lookupAux(key);
  return value;
}
bool SimpleOverlayDBDelegate::exists(const h256 &key) {
  sharedLock lock(shared_mutex_);
  return odb_->existsAux(key);
}
void SimpleOverlayDBDelegate::commit() {
  boost::unique_lock lock(shared_mutex_);
  odb_->commit();
}

SimpleOverlayDBDelegate::SimpleOverlayDBDelegate(const std::string &path,
                                                 bool overwrite)
    : odb_(std::make_shared<dev::OverlayDB>(dev::OverlayDB(
          dev::eth::State::openDB(path, TEMP_GENESIS_HASH,
                                  overwrite ? dev::WithExisting::Kill
                                            : dev::WithExisting::Trust)))) {}