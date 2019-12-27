#ifndef TARAXA_NODE_NET_NET_H_
#define TARAXA_NODE_NET_NET_H_

#include "../full_node.hpp"
#include "NetFace.h"

namespace taraxa::net {

class Net : public NetFace {
 public:
  Net(std::shared_ptr<taraxa::FullNode>& _full_node);
  virtual RPCModules implementedModules() const override {
    return RPCModules{RPCModule{"net", "1.0"}};
  }
  virtual std::string net_version() override;
  virtual std::string net_peerCount() override;
  virtual bool net_listening() override;

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
};

}  // namespace taraxa::net

#endif