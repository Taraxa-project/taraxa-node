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
  explicit Taraxa(std::shared_ptr<taraxa::FullNode> const& _full_node);

  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"taraxa", "1.0"}}; }

  virtual std::string taraxa_protocolVersion() override;
  virtual Json::Value taraxa_getVersion() override;
  virtual Json::Value taraxa_getDagBlockByHash(std::string const& _blockHash, bool _includeTransactions) override;
  virtual Json::Value taraxa_getDagBlockByLevel(std::string const& _blockLevel, bool _includeTransactions) override;
  virtual std::string taraxa_dagBlockLevel() override;
  virtual std::string taraxa_dagBlockPeriod() override;
  virtual Json::Value taraxa_getScheduleBlockByPeriod(std::string const& _period) override;
  Json::Value taraxa_getConfig() override;
  Json::Value taraxa_queryDPOS(Json::Value const& _q) override;

 protected:
  std::weak_ptr<taraxa::FullNode> full_node_;

 private:
  using NodePtr = decltype(full_node_.lock());
  Json::Value version;

  NodePtr tryGetNode();
};

}  // namespace taraxa::net
