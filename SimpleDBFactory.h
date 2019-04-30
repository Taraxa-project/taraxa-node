//
// Created by JC on 2019-04-27.
//
#ifndef TARAXA_NODE_SIMPLEDBFACTORY_H
#define TARAXA_NODE_SIMPLEDBFACTORY_H

#include "SimpleOverlayDBDelegate.h"
#include "SimpleTaraxaRocksDBDelegate.h"
#include "SimpleStateDBDelegate.h"

class SimpleDBFace;

class SimpleDBFactory {
public:
    SimpleDBFactory() = delete;
    ~SimpleDBFactory() = delete;
    enum SimpleDBType {
        TaraxaRocksDBKind,
        OverlayDBKind,
        StateDBKind
    };

    static std::shared_ptr<SimpleDBFace> createDelegate(const SimpleDBType type, const std::string &path) {
      switch(type) {
        case TaraxaRocksDBKind:
          return std::make_shared<SimpleTaraxaRocksDBDelegate>(path);
        case StateDBKind:
          return std::make_shared<SimpleStateDBDelegate>(path);
        case OverlayDBKind:
          return std::make_shared<SimpleOverlayDBDelegate>(path);
        default:
          assert(false);
      }
    }
};
#endif //TARAXA_NODE_SIMPLEDBFACTORY_H
