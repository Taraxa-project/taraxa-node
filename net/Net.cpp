#include "Net.h"
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>
#include <libethcore/Common.h>
#include <libwebthree/WebThree.h>
#include "full_node.hpp"
#include "network.hpp"

using namespace dev;
using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace eth;
using namespace shh;

namespace taraxa::net {

Net::Net(std::shared_ptr<taraxa::FullNode> &_full_node)
    : full_node_(_full_node) {}

std::string Net::net_version() {
  if (auto full_node = full_node_.lock()) {
    return full_node->getNetwork()->getConfig().network_id;
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

}