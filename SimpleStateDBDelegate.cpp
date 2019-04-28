//
// Created by JC on 2019-04-26.
//
/*
#include "SimpleDBFace.h"
using State = dev::eth::State;

class SimpleStateDBDelegate : public SimpleDBFace {
    bool put override (const std::string &key, const std::string &value) {
      // TODO:
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
    void commit override () {
      // TODO:
    }
public:
    static std::shared_ptr<SimpleDBFace> makeShared(const std::string& path) {
      return std::make_shared<SimpleStateDBDelegate>(path);
    }
    SimpleStateDBDelegate(const std::string& path) :
            state(std::make_shared<State>(0, dev::eth::State::openDB(path, TEMP_GENESIS_HASH,
                    dev::WithExisting::Kill),dev::eth::BaseState::Empty)){}
private:
    std::shared_ptr<State> state;
};
 */