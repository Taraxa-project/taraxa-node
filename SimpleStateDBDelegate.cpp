//
// Created by JC on 2019-04-26.
//
#include "SimpleStateDBDelegate.h"

bool SimpleStateDBDelegate::put (const std::string &key, const std::string &value) {
  const dev::Address id = stringToAddress(key);
  if(state->addressInUse(id)) {
    return false;
  }
  state->setBalance(id, stringToBalance(value));
  return true;
}
bool SimpleStateDBDelegate::update (const std::string &key, const std::string &value) {
  state->setBalance(stringToAddress(key), stringToBalance(value));
  return true;
}
std::string SimpleStateDBDelegate::get (const std::string &key) {
  const dev::Address id = stringToAddress(key);
  if(!state->addressInUse(id)) {
    return "";
  }
  return std::to_string(state->balance(id));
}
void SimpleStateDBDelegate::commit() {
  // TODO: perhaps we shall let the CommitBehaviour be flexible
  state->commit(dev::eth::State::CommitBehaviour::KeepEmptyAccounts);
}
SimpleStateDBDelegate::SimpleStateDBDelegate(const std::string& path) :
            state(std::make_shared<dev::eth::State>(0, dev::eth::State::openDB(path, TEMP_GENESIS_HASH,
                    dev::WithExisting::Kill),dev::eth::BaseState::Empty)){}