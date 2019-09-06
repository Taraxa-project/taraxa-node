#ifndef TARAXA_NODE_SIMPLE_DB_FACTORY_HPP
#define TARAXA_NODE_SIMPLE_DB_FACTORY_HPP

#include "simple_overlaydb_delegate.hpp"

class SimpleDBFace;

class SimpleDBFactory {
 public:
  SimpleDBFactory() = delete;
  ~SimpleDBFactory() = delete;

  template <typename T>
  static std::unique_ptr<T> createDelegate(const std::string &path,
                                           bool overwrite) {
    return std::make_unique<T>(path, overwrite);
  }
};
#endif  // TARAXA_NODE_SIMPLE_DB_FACTORY_HPP
