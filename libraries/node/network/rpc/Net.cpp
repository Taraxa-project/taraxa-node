#include "Net.h"

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>

#include "network/network.hpp"
#include "node/node.hpp"

using namespace dev;
using namespace std;
using namespace jsonrpc;

namespace taraxa::net {

Net::Net(std::shared_ptr<taraxa::FullNode> const& _full_node) : full_node_(_full_node) {}

std::string Net::net_version() {
  if (auto full_node = full_node_.lock()) {
    return toString(full_node->getConfig().network.network_id);
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

std::string Net::net_peerCount() {
  if (auto full_node = full_node_.lock()) {
    return toJS(full_node->getNetwork()->getPeerCount());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

bool Net::net_listening() {
  if (auto full_node = full_node_.lock()) {
    return full_node->getNetwork()->isStarted();
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

}  // namespace taraxa::net