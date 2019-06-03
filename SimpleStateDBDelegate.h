/*
 * @Copyright: Taraxa.io
 * @Author: JC
 * @Date: 2019-04-28 15:45:35
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 16:12:10
 */

#ifndef TARAXA_NODE_SIMPLESTATEDBDELEGATE_H
#define TARAXA_NODE_SIMPLESTATEDBDELEGATE_H
#include "SimpleDBFace.h"

class SimpleStateDBDelegate : public SimpleDBFace {
 public:
  bool put(const std::string &key, const std::string &value) override;
  bool update(const std::string &key, const std::string &value) override;
  std::string get(const std::string &key) override;
  void commit() override;
  SimpleStateDBDelegate(const std::string &path, bool overwrite);

 private:
  static dev::Address stringToAddress(const std::string &s) {
    return dev::Address(s);
  }
  static taraxa::bal_t stringToBalance(const std::string &s) {
    return (taraxa::bal_t)std::stoull(s);
  }

  std::shared_ptr<dev::eth::State> state_ = nullptr;
};
#endif  // TARAXA_NODE_SIMPLESTATEDBDELEGATE_H
