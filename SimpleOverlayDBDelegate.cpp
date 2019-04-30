//
// Created by JC on 2019-04-26.
//
#include "SimpleOverlayDBDelegate.h"

bool SimpleOverlayDBDelegate::put (const std::string &key, const std::string &value) {
  const h256 hashKey = stringToHashKey(key);
  if(odb->exists(hashKey)) {
    return false;
  }
  odb->insert(hashKey, value);
  return true;
}
bool SimpleOverlayDBDelegate::update (const std::string &key, const std::string &value) {
  odb->insert(stringToHashKey(key), value);
  return true;
}
std::string SimpleOverlayDBDelegate::get (const std::string &key) {
  const h256 hashKey = stringToHashKey(key);
  if(!odb->exists(hashKey)) {
    return "";
  }
  return odb->lookup(hashKey);
}
void SimpleOverlayDBDelegate::commit() {
  odb->commit();
}

SimpleOverlayDBDelegate::SimpleOverlayDBDelegate(const std::string& path):odb(std::make_shared<dev::OverlayDB>(
        dev::OverlayDB(dev::eth::State::openDB(path, TEMP_GENESIS_HASH,dev::WithExisting::Kill)))){}