/*
 * @Copyright: Taraxa.io
 * @Author: JC
 * @Date: 2019-05-15 15:47:47
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 16:18:22
 */

#include "SimpleStateDBDelegate.h"

bool SimpleStateDBDelegate::put(const std::string &key,
                                const std::string &value) {
  const dev::Address id = stringToAddress(key);
  upgradableLock lock(shared_mutex_);
  if (state_->addressInUse(id)) {
    return false;
  }
  upgradeLock locked(lock);
  state_->setBalance(id, stringToBalance(value));
  return true;
}
bool SimpleStateDBDelegate::update(const std::string &key,
                                   const std::string &value) {
  boost::unique_lock lock(shared_mutex_);
  state_->setBalance(stringToAddress(key), stringToBalance(value));
  return true;
}
std::string SimpleStateDBDelegate::get(const std::string &key) {
  const dev::Address id = stringToAddress(key);
  sharedLock lock(shared_mutex_);
  if (!state_->addressInUse(id)) {
    return "";
  }
  return std::to_string(state_->balance(id));
}
void SimpleStateDBDelegate::commit() {
  // TODO: perhaps we shall let the CommitBehaviour be flexible
  boost::unique_lock lock(shared_mutex_);
  state_->commit(dev::eth::State::CommitBehaviour::KeepEmptyAccounts);
}
SimpleStateDBDelegate::SimpleStateDBDelegate(const std::string &path)
    : state_(std::make_shared<dev::eth::State>(
          0,
          dev::eth::State::openDB(path, TEMP_GENESIS_HASH,
                                  dev::WithExisting::Kill),
          dev::eth::BaseState::Empty)) {}