//
// Created by JC on 2019-04-27.
//
#ifndef TARAXA_NODE_SIMPLEDBFACTORY_H
#define TARAXA_NODE_SIMPLEDBFACTORY_H

#include "SimpleOverlayDBDelegate.h"
#include "SimpleStateDBDelegate.h"
#include "SimpleTaraxaRocksDBDelegate.h"

class SimpleDBFace;

class SimpleDBFactory {
 public:
  SimpleDBFactory() = delete;
  ~SimpleDBFactory() = delete;

  template <typename T>
  static std::shared_ptr<T> createDelegate(const std::string &path,
                                           bool overwrite) {
    return std::make_shared<T>(path, overwrite);
  }
};
#endif  // TARAXA_NODE_SIMPLEDBFACTORY_H
