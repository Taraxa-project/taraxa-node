#pragma once

#include "NetFace.h"
#include "common/app_base.hpp"
namespace taraxa {
class FullNode;
}

namespace taraxa::net {

class Net : public NetFace {
 public:
  explicit Net(std::shared_ptr<taraxa::AppBase> const& app);
  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"net", "1.0"}}; }
  virtual std::string net_version() override;
  virtual std::string net_peerCount() override;
  virtual bool net_listening() override;

 private:
  std::weak_ptr<taraxa::AppBase> app_;
};

}  // namespace taraxa::net
