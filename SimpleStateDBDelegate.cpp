//
// Created by JC on 2019-04-26.
//
#include "SimpleStateDBDelegate.h"

bool SimpleStateDBDelegate::put (const std::string &key, const std::string &value) {
  //TODO: fix
  return true;
}
bool SimpleStateDBDelegate::update (const std::string &key, const std::string &value) {
  //TODO: fix
  return true;
}
std::string SimpleStateDBDelegate::get (const std::string &key) {
  //TODO: fix
  return "";
}
void SimpleStateDBDelegate::commit() {
  //TODO: fix
}
SimpleStateDBDelegate::SimpleStateDBDelegate(const std::string& path) :
            state(std::make_shared<dev::eth::State>(0, dev::eth::State::openDB(path, TEMP_GENESIS_HASH,
                    dev::WithExisting::Kill),dev::eth::BaseState::Empty)){}