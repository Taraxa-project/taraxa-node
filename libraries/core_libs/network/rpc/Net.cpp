#include "Net.h"

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/CommonJS.h>

#include "network/network.hpp"

using namespace dev;
using namespace std;
using namespace jsonrpc;

namespace taraxa::net {

Net::Net(std::shared_ptr<taraxa::AppBase> const& app) : app_(app) {}

std::string Net::net_version() {
  if (auto app = app_.lock()) {
    return toString(app->getConfig().genesis.chain_id);
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

std::string Net::net_peerCount() {
  if (auto app = app_.lock()) {
    return toJS(app->getNetwork()->getPeerCount());
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

bool Net::net_listening() {
  if (auto app = app_.lock()) {
    return app->getNetwork()->isStarted();
  }
  BOOST_THROW_EXCEPTION(JsonRpcException(Errors::ERROR_RPC_INTERNAL_ERROR));
}

}  // namespace taraxa::net