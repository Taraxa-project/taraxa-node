#include "AdminNet.h"
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>
#include <libethcore/Common.h>
#include <libwebthree/WebThree.h>
#include "JsonHelperTaraxa.h"
#include "SessionManager.h"

using namespace std;
using namespace dev;
using namespace dev::rpc;

AdminNet::AdminNet(std::shared_ptr<taraxa::FullNode>& _full_node)
    : full_node_(_full_node) {}

bool AdminNet::admin_net_connect(std::string const& _node,
                                 std::string const& _session) {
  return false;
}

Json::Value AdminNet::admin_net_peers(std::string const& _session) {
  return "";
}

Json::Value AdminNet::admin_net_nodeInfo(std::string const& _session) {
  return "";
}

Json::Value AdminNet::admin_nodeInfo() { return ""; }

Json::Value AdminNet::admin_peers() { return ""; }

bool AdminNet::admin_addPeer(string const& _node) { return false; }
