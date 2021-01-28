#pragma once

#include "NetFace.h"

namespace taraxa {
class FullNode;
}

namespace taraxa::net {

class Net : public NetFace {
 public:
  explicit Net(std::shared_ptr<taraxa::FullNode> const& _full_node);
  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"net", "1.0"}}; }
  virtual std::string net_version() override;
  virtual std::string net_peerCount() override;
  virtual bool net_listening() override;

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
};

}  // namespace taraxa::net
