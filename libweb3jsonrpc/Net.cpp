/*
        This file is part of cpp-ethereum.

        cpp-ethereum is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        cpp-ethereum is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Net.cpp
 * @authors:
 *   Gav Wood <i@gavwood.com>
 *   Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#include "Net.h"
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server.h>
#include <libdevcore/CommonData.h>
#include <libdevcore/CommonJS.h>
#include <libethcore/Common.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <libwebthree/WebThree.h>
#include "full_node.hpp"
#include "network.hpp"

using namespace dev;
using namespace std;
using namespace jsonrpc;
using namespace dev;
using namespace eth;
using namespace shh;
using namespace dev::rpc;

Net::Net(std::shared_ptr<taraxa::FullNode> &_full_node)
    : full_node_(_full_node) {}

std::string Net::net_version() {
  if (auto full_node = full_node_.lock()) {
    return toString(full_node->getNetwork()->getConfig().network_id);
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