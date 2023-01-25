
#pragma once

#include <future>

#include "DebugFace.h"
#include "node/node.hpp"

namespace dev::eth {
class Client;
}

namespace taraxa::net {

class Debug : public DebugFace {
 public:
  explicit Debug(const std::shared_ptr<taraxa::FullNode>& _full_node)
      : full_node_(_full_node) {}
  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"debug", "1.0"}}; }

  virtual Json::Value debug_traceTransaction(const std::string& param1, const Json::Value& param2) override;
  virtual Json::Value debug_traceCall(const Json::Value& param1, const std::string& param2, const Json::Value& param3) override;

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
  const uint64_t kChainId;
};

}  // namespace taraxa::net