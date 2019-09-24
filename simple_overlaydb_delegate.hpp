#ifndef TARAXA_NODE_SIMPLE_OVERLAYDB_DELEGATE_HPP
#define TARAXA_NODE_SIMPLE_OVERLAYDB_DELEGATE_HPP

#include "libdevcore/OverlayDB.h"
#include "simple_db_face.hpp"

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
  SimpleOverlayDBDelegate(const std::string &path, bool overwrite);

 private:
  static h256 stringToHashKey(const std::string &s) { return h256(s); }

  std::shared_ptr<dev::OverlayDB> odb_ = nullptr;
};
#endif  // TARAXA_NODE_SIMPLE_OVERLAYDB_DELEGATE_HPP
