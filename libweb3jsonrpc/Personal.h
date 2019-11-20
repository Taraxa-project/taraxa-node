#pragma once
#include "../full_node.hpp"
#include "PersonalFace.h"

namespace dev {

namespace eth {
class KeyManager;
class AccountHolder;
class Interface;
}  // namespace eth

namespace rpc {

class Personal : public dev::rpc::PersonalFace {
 public:
  Personal(std::shared_ptr<taraxa::FullNode>& _full_node);
  virtual RPCModules implementedModules() const override {
    return RPCModules{RPCModule{"personal", "1.0"}};
  }
  virtual std::string personal_newAccount(
      std::string const& _password) override;
  virtual bool personal_unlockAccount(std::string const& _address,
                                      std::string const& _password,
                                      int _duration) override;
  virtual std::string personal_signAndSendTransaction(
      Json::Value const& _transaction, std::string const& _password) override;
  virtual std::string personal_sendTransaction(
      Json::Value const& _transaction, std::string const& _password) override;
  virtual Json::Value personal_listAccounts() override;

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
};

}  // namespace rpc
}  // namespace dev
