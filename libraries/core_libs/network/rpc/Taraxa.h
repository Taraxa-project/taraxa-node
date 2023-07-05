#pragma once

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/Common.h>

#include <iosfwd>
#include <memory>
#include <optional>

#include "TaraxaFace.h"
#include "node/node.hpp"

namespace taraxa::net {

class Taraxa : public TaraxaFace {
 public:
  explicit Taraxa(const std::shared_ptr<taraxa::FullNode>& _full_node);

  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"taraxa", "1.0"}}; }

  virtual std::string taraxa_protocolVersion() override;
  virtual Json::Value taraxa_getVersion() override;
  virtual Json::Value taraxa_getDagBlockByHash(const std::string& _blockHash, bool _includeTransactions) override;
  virtual Json::Value taraxa_getDagBlockByLevel(const std::string& _blockLevel, bool _includeTransactions) override;
  virtual std::string taraxa_dagBlockLevel() override;
  virtual std::string taraxa_dagBlockPeriod() override;
  virtual Json::Value taraxa_getScheduleBlockByPeriod(const std::string& _period) override;
  virtual std::string taraxa_pbftBlockHashByPeriod(const std::string& _period) override;
  virtual Json::Value taraxa_getConfig() override;
  virtual Json::Value taraxa_getChainStats() override;

 protected:
  std::weak_ptr<taraxa::FullNode> full_node_;

 private:
  using NodePtr = decltype(full_node_.lock());
  Json::Value version;

  NodePtr tryGetNode();
};

}  // namespace taraxa::net
