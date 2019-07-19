/*
 * @Copyright: Taraxa.io
 * @Author: JC
 * @Date: 2019-05-15 15:47:47
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 16:18:22
 */

#include "SimpleStateDBDelegate.h"
#include "types.hpp"

using namespace std;
using namespace taraxa;
using namespace dev::eth;

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
  return taraxa::toString(state_->balance(id));
}

root_t SimpleStateDBDelegate::commitToTrie(
    bool flush, State::CommitBehaviour const &commit_behaviour) {
  boost::unique_lock lock(shared_mutex_);
  state_->commit(commit_behaviour);
  if (flush) {
    state_->db().commit();
  }
  return state_->rootHash();
}

void SimpleStateDBDelegate::flushTrie() {
  boost::unique_lock lock(shared_mutex_);
  state_->db().commit();
}

void SimpleStateDBDelegate::setRoot(root_t const &root) {
  boost::unique_lock lock(shared_mutex_);
  state_->setRoot(root);
}

SimpleStateDBDelegate::SimpleStateDBDelegate(const std::string &path,
                                             bool overwrite)
    : state_(std::make_shared<dev::eth::State>(
          0,
          dev::eth::State::openDB(
              path, TEMP_GENESIS_HASH,
              overwrite ? dev::WithExisting::Kill : dev::WithExisting::Trust),
          dev::eth::BaseState::Empty)) {}