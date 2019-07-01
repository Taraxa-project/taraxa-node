/*
 * @Copyright: Taraxa.io
 * @Author: JC
 * @Date: 2019-05-15 15:47:47
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 16:18:22
 */

#include "SimpleStateDBDelegate.h"
#include "util_eth.hpp"

using namespace std;
using namespace taraxa::util::eth;
using namespace dev;
using namespace dev::eth;

bool SimpleStateDBDelegate::put(const string &key, const string &value) {
  const Address id = stringToAddress(key);
  upgradableLock lock(shared_mutex_);
  if (state_->addressInUse(id)) {
    return false;
  }
  upgradeLock locked(lock);
  state_->setBalance(id, stringToBalance(value));
  return true;
}
bool SimpleStateDBDelegate::update(const string &key, const string &value) {
  boost::unique_lock lock(shared_mutex_);
  state_->setBalance(stringToAddress(key), stringToBalance(value));
  return true;
}
string SimpleStateDBDelegate::get(const string &key) {
  const Address id = stringToAddress(key);
  sharedLock lock(shared_mutex_);
  if (!state_->addressInUse(id)) {
    return "";
  }
  return to_string(state_->balance(id));
}

void SimpleStateDBDelegate::commit() {
  // TODO: perhaps we shall let the CommitBehaviour be flexible
  boost::unique_lock lock(shared_mutex_);
  state_->commit(State::CommitBehaviour::KeepEmptyAccounts);
}

SimpleStateDBDelegate::SimpleStateDBDelegate(const OverlayAndRawDB &dbs)
    : state_(make_shared<State>(0, dbs.overlayDB, BaseState::Empty)),
      raw_db_(dbs.rawDB) {}

SimpleStateDBDelegate::SimpleStateDBDelegate(const string &path, bool overwrite)
    : SimpleStateDBDelegate(
          newOverlayDB(path, TEMP_GENESIS_HASH,
                       overwrite ? WithExisting::Kill : WithExisting::Trust)) {}