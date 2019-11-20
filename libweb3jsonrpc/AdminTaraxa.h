#pragma once
#include <libethcore/Common.h>
#include "../full_node.hpp"
#include "AdminTaraxaFace.h"

namespace dev {

namespace eth {
class Client;
class TrivialGasPricer;
class KeyManager;
}  // namespace eth

namespace rpc {

class SessionManager;

class AdminTaraxa : public AdminTaraxaFace {
 public:
  AdminTaraxa(std::shared_ptr<taraxa::FullNode>& _full_node);

  virtual RPCModules implementedModules() const override {
    return RPCModules{RPCModule{"admin", "1.0"}, RPCModule{"miner", "1.0"}};
  }

  virtual bool admin_taraxa_setMining(bool _on,
                                      std::string const& _session) override;
  virtual Json::Value admin_taraxa_blockQueueStatus(
      std::string const& _session) override;
  virtual bool admin_taraxa_setAskPrice(std::string const& _wei,
                                        std::string const& _session) override;
  virtual bool admin_taraxa_setBidPrice(std::string const& _wei,
                                        std::string const& _session) override;
  virtual Json::Value admin_taraxa_findBlock(
      std::string const& _blockHash, std::string const& _session) override;
  virtual std::string admin_taraxa_blockQueueFirstUnknown(
      std::string const& _session) override;
  virtual bool admin_taraxa_blockQueueRetryUnknown(
      std::string const& _session) override;
  virtual Json::Value admin_taraxa_allAccounts(
      std::string const& _session) override;
  virtual Json::Value admin_taraxa_newAccount(
      const Json::Value& _info, std::string const& _session) override;
  virtual bool admin_taraxa_setMiningBenefactor(
      std::string const& _uuidOrAddress, std::string const& _session) override;
  virtual Json::Value admin_taraxa_inspect(
      std::string const& _address, std::string const& _session) override;
  virtual Json::Value admin_taraxa_reprocess(
      std::string const& _blockNumberOrHash,
      std::string const& _session) override;
  virtual Json::Value admin_taraxa_vmTrace(
      std::string const& _blockNumberOrHash, int _txIndex,
      std::string const& _session) override;
  virtual Json::Value admin_taraxa_getReceiptByHashAndIndex(
      std::string const& _blockNumberOrHash, int _txIndex,
      std::string const& _session) override;
  virtual bool miner_start(int _threads) override;
  virtual bool miner_stop() override;
  virtual bool miner_setEtherbase(std::string const& _uuidOrAddress) override;
  virtual bool miner_setExtra(std::string const& _extraData) override;
  virtual bool miner_setGasPrice(std::string const& _gasPrice) override;
  virtual std::string miner_hashrate() override;

  virtual void setMiningBenefactorChanger(
      std::function<void(Address const&)> const& _f) {}

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
};

}  // namespace rpc
}  // namespace dev
