#include "jsonrpc_ws_server.hpp"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>

#include <string_view>

#include "common/jsoncpp.hpp"
#include "common/util.hpp"
#include "config/config.hpp"

namespace taraxa::net {

std::string JsonRpcWsSession::processRequest(const std::string_view &request) {
  Json::Value json;
  try {
    json = util::parse_json(request);
  } catch (Json::Exception const &e) {
    LOG(log_er_) << "Failed to parse" << e.what();
    closed_ = true;
    return {};
  }

  // Check for Batch requests
  if (!json.isArray()) {
    if (const auto method = json.get("method", ""); method == "eth_subscribe") {
      return handleSubscription(json);
    }
  }

  return handleRequest(json);
}

std::string JsonRpcWsSession::handleRequest(const Json::Value &req) {
  std::string response;
  auto ws_server = ws_server_.lock();
  if (ws_server) {
    auto handler = ws_server->GetHandler();
    if (handler != NULL) {
      try {
        handler->HandleRequest(util::to_string(req), response);
      } catch (std::exception const &e) {
        LOG(log_er_) << "Exception " << e.what();
        Json::Value json_response;
        auto &res_json_error = json_response["error"] = Json::Value(Json::objectValue);
        res_json_error["code"] = jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR;
        res_json_error["message"] = e.what();
        json_response["id"] = req.get("id", 0);
        json_response["jsonrpc"] = "2.0";
        return util::to_string(json_response);
      }
    }
  }
  return response;
}

std::string JsonRpcWsSession::handleSubscription(const Json::Value &req) {
  Json::Value json_response;

  const auto params = req.get("params", Json::Value(Json::Value(Json::arrayValue)));

  json_response["id"] = req.get("id", 0);
  ;
  json_response["jsonrpc"] = "2.0";
  const auto subscription_id = subscription_id_.fetch_add(1) + 1;

  if (params.size() > 0) {
    if (params[0].asString() == "newHeads") {
      new_heads_subscription_ = subscription_id;
    } else if (params[0].asString() == "newPendingTransactions") {
      new_transactions_subscription_ = subscription_id;
    } else if (params[0].asString() == "newDagBlocks") {
      new_dag_blocks_subscription_ = subscription_id;
    } else if (params[0].asString() == "newDagBlocksFinalized") {
      new_dag_block_finalized_subscription_ = subscription_id;
    } else if (params[0].asString() == "newPbftBlocks") {
      new_pbft_block_executed_subscription_ = subscription_id;
    } else if (params[0].asString() == "newPillarBlocks") {
      new_pillar_block_subscription_ = subscription_id;
    }
  }

  json_response["result"] = dev::toJS(subscription_id);

  return util::to_string(json_response);
}

std::shared_ptr<WsSession> JsonRpcWsServer::createSession(tcp::socket &&socket) {
  return std::make_shared<JsonRpcWsSession>(std::move(socket), node_addr_, shared_from_this());
}

}  // namespace taraxa::net