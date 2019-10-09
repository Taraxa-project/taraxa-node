#ifndef TARAXA_NODE_SIMPLE_OVERLAYDB_DELEGATE_HPP
#define TARAXA_NODE_SIMPLE_OVERLAYDB_DELEGATE_HPP

#include "libdevcore/OverlayDB.h"
#include "simple_db_face.hpp"
#include "util.hpp"

class SimpleOverlayDBDelegate : public SimpleDBFace {
 public:
  bool put(const std::string &key, const std::string &value) override;
  bool update(const std::string &key, const std::string &value) override;
  std::string get(const std::string &key) override;
  bool put(const h256 &key, const dev::bytes &value) override;
  bool update(const h256 &key, const dev::bytes &value) override;
  dev::bytes get(const h256 &key) override;
  bool exists(const h256 &key) override;
  void commit() override;
  void forEach(std::function<bool(std::string, std::string)> f) override {
    raw_db_->forEach([&](auto const &k, auto const &v) {
      return f(k.toString(), v.toString());
    });
  }

  SimpleOverlayDBDelegate(const std::string &path, bool overwrite,
                          uint32_t binary_cache_size = 1000,
                          uint32_t string_cache_size = 1000);

 private:
  static h256 stringToHashKey(const std::string &s) { return h256(s); }

  dev::db::DatabaseFace *raw_db_ = nullptr;
  std::shared_ptr<dev::OverlayDB> odb_ = nullptr;
  ExpirationCacheMap<std::string, std::string> string_cache_;
  ExpirationCacheMap<h256, dev::bytes> binary_cache_;
  uint32_t binary_cache_size_;
  uint32_t string_cache_size_;
};
#endif  // TARAXA_NODE_SIMPLE_OVERLAYDB_DELEGATE_HPP
