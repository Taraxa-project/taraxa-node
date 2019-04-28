//
// Created by JC on 2019-04-27.
//
#ifndef TARAXA_NODE_SIMPLEDBFACTORY_H
#define TARAXA_NODE_SIMPLEDBFACTORY_H

//#include "SimpleDBFace.h"
//#include "SimpleOverlayDBDelegate.cpp"
#include "SimpleTaraxaRocksDBDelegate.h"
//#include "SimpleStateDBDelegate.cpp"

class SimpleDBFace;

class SimpleDBFactory {
public:
    enum SimpleDBType {
        TaraxaRocksDBKind,
        OverlayDBKind,
        StateDBKind
    };

    static std::shared_ptr<SimpleDBFace> createDelegate(const SimpleDBType type, const std::string &path) {
      switch(type) {
        case TaraxaRocksDBKind:
          return std::make_shared<SimpleTaraxaRocksDBDelegate>(path);
          //return SimpleTaraxaRocksDBDelegate::makeShared(path);
        case StateDBKind:
          //return std::make_shared<SimpleStateDBDelegate>(path);
          //return SimpleStateDBDelegate::makeShared(path);
        case OverlayDBKind:
          //return std::make_shared<SimpleOverlayDBDelegate>(path);
          //return SimpleOverlayDBDelegate::makeShared(path);
        default:
          assert(false);
      }
    }
};
#endif //TARAXA_NODE_SIMPLEDBFACTORY_H
