//
// Created by JC on 2019-04-26.
//
/*
#include "SimpleDBFace.h"

using OverlayDB = dev::OverlayDB;
class SimpleOverlayDBDelegate : public SimpleDBFace {
public:
    bool put override (const std::string &key, const std::string &value) {
      return true;
    }
    bool update override (const std::string &key, const std::string &value) {
      // TODO:
      return true;
    }
    std::string get override (const std::string &key) {
      // TODO:
      return "";
    }
    void commit() override {
      // TODO:
    }
    static std::shared_ptr<SimpleDBFace> makeShared(const std::string& path) {
      return std::make_shared<SimpleOverlayDBDelegate>(path);
    }
    SimpleOverlayDBDelegate(const std::string& path):odb(std::make_shared<OverlayDB>(OverlayDB(
            dev::eth::State::openDB(path, TEMP_GENESIS_HASH,dev::WithExisting::Kill)))){}
    std::shared_ptr<OverlayDB> odb;
};
 */