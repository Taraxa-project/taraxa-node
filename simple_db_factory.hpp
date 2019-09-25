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
                                           bool overwrite,
                                           uint32_t binary_cache_size = 100,
                                           uint32_t string_cache_size = 100) {
    return std::make_unique<T>(path, overwrite, binary_cache_size, string_cache_size);
  }
};
#endif  // TARAXA_NODE_SIMPLE_DB_FACTORY_HPP
