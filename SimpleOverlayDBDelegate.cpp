//
// Created by JC on 2019-04-26.
//
#include "SimpleOverlayDBDelegate.h"

bool SimpleOverlayDBDelegate::put (const std::string &key, const std::string &value) {
  //TODO: fix
  return true;
}
bool SimpleOverlayDBDelegate::update (const std::string &key, const std::string &value) {
  //TODO: fix
  return true;
}
std::string SimpleOverlayDBDelegate::get (const std::string &key) {
  //TODO: fix
  return "";
}
void SimpleOverlayDBDelegate::commit() {
  //TODO: fix
}

SimpleOverlayDBDelegate::SimpleOverlayDBDelegate(const std::string& path):odb(std::make_shared<dev::OverlayDB>(
        dev::OverlayDB(dev::eth::State::openDB(path, TEMP_GENESIS_HASH,dev::WithExisting::Kill)))){}