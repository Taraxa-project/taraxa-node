#pragma once

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/Common.h>

#include <memory>

#include "TaraxaFace.h"
#include "common/app_base.hpp"
#include "libweb3jsonrpc/ModularServer.h"

namespace taraxa::net {

class Taraxa : public TaraxaFace {
 public:
  explicit Taraxa(std::shared_ptr<taraxa::AppBase> app);

  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"taraxa", "1.0"}}; }

  virtual std::string taraxa_protocolVersion() override;
  virtual Json::Value taraxa_getVersion() override;
  virtual Json::Value taraxa_getDagBlockByHash(const std::string& _blockHash, bool _includeTransactions) override;
  virtual Json::Value taraxa_getDagBlockByLevel(const std::string& _blockLevel, bool _includeTransactions) override;
  virtual std::string taraxa_dagBlockLevel() override;
  virtual std::string taraxa_dagBlockPeriod() override;
  virtual Json::Value taraxa_getScheduleBlockByPeriod(const std::string& _period) override;
  virtual Json::Value taraxa_getNodeVersions() override;
  virtual std::string taraxa_pbftBlockHashByPeriod(const std::string& _period) override;
  virtual Json::Value taraxa_getConfig() override;
  virtual Json::Value taraxa_getChainStats() override;
  virtual std::string taraxa_yield(const std::string& _period) override;
  virtual std::string taraxa_totalSupply(const std::string& _period) override;
  virtual Json::Value taraxa_getPillarBlockData(const std::string& pillar_block_period,
                                                bool include_signatures) override;

 protected:
  std::weak_ptr<taraxa::AppBase> app_;

 private:
  Json::Value version;

  std::shared_ptr<taraxa::AppBase> tryGetApp();
};

}  // namespace taraxa::net
