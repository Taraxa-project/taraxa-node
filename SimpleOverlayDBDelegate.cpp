/*
 * @Copyright: Taraxa.io
 * @Author: JC
 * @Date: 2019-05-15 15:15:02
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 15:49:08
 */

#include "SimpleOverlayDBDelegate.h"

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
  if (!odb_->exists(hashKey)) {
    return "";
  }
  return odb_->lookup(hashKey);
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