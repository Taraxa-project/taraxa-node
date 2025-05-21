#include "jsonrpc_ws_server.hpp"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <jsonrpccpp/common/exception.h>
#include <libdevcore/CommonJS.h>

#include <memory>
#include <string_view>

#include "common/jsoncpp.hpp"
#include "network/rpc/eth/LogFilter.hpp"
#include "network/subscriptions.hpp"

namespace taraxa::net {

std::string JsonRpcWsSession::processRequest(const std::string_view &req_str) {
  Json::Value req;
  try {
    req = util::parse_json(req_str);
  } catch (Json::Exception const &e) {
    LOG(log_er_) << "Failed to parse" << e.what();
    return {};
  }

  try {
    // Check for Batch requests
    if (!req.isArray()) {
      if (const auto method = req.get("method", ""); method == "eth_subscribe") {
        return handleSubscription(req);
      }
      if (const auto method = req.get("method", ""); method == "eth_unsubscribe") {
        return handleUnsubscription(req);
      }
    }

    return handleRequest(req);
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

std::string JsonRpcWsSession::handleRequest(const Json::Value &req) {
  std::string response;
  auto ws_server = ws_server_.lock();
  if (ws_server) {
    auto handler = ws_server->GetHandler();
    if (handler != NULL) {
      handler->HandleRequest(util::to_string(req), response);
    }
  }
  return response;
}

std::string JsonRpcWsSession::handleSubscription(const Json::Value &req) {
  Json::Value json_response;

  const auto params = req.get("params", Json::Value(Json::Value(Json::arrayValue)));

  json_response["id"] = req.get("id", 0);
  json_response["jsonrpc"] = "2.0";
  const auto subscription_id = subscription_id_.fetch_add(1) + 1;
  Json::Value options;
  if (params.size() == 2) {
    options = params[1];
  }

  if (params.size() > 0) {
    if (params[0].asString() == "newHeads") {
      subscriptions_.addSubscription(std::make_shared<HeadsSubscription>(subscription_id));
    } else if (params[0].asString() == "newPendingTransactions") {
      subscriptions_.addSubscription(std::make_shared<TransactionsSubscription>(subscription_id));
    } else if (params[0].asString() == "newDagBlocks") {
      subscriptions_.addSubscription(std::make_shared<DagBlocksSubscription>(subscription_id, options.asBool()));
    } else if (params[0].asString() == "newDagBlocksFinalized") {
      subscriptions_.addSubscription(std::make_shared<DagBlockFinalizedSubscription>(subscription_id));
    } else if (params[0].asString() == "newPbftBlocks") {
      subscriptions_.addSubscription(
          std::make_shared<PbftBlockExecutedSubscription>(subscription_id, options.asBool()));
    } else if (params[0].asString() == "newPillarBlockData") {
      subscriptions_.addSubscription(std::make_shared<PillarBlockSubscription>(subscription_id, options.asBool()));
    } else if (params[0].asString() == "logs") {
      auto filter =
          rpc::eth::LogFilter(0, std::nullopt, rpc::eth::parse_addresses(options), rpc::eth::parse_topics(options));
      subscriptions_.addSubscription(std::make_shared<LogsSubscription>(subscription_id, std::move(filter)));
    } else {
      throw std::runtime_error("Unknown subscription type: " + params[0].asString());
    }
  }

  json_response["result"] = dev::toJS(subscription_id);

  return util::to_string(json_response);
}

std::string JsonRpcWsSession::handleUnsubscription(const Json::Value &req) {
  Json::Value json_response;
  json_response["id"] = req.get("id", 0);
  json_response["jsonrpc"] = "2.0";

  const auto params = req.get("params", Json::Value(Json::Value(Json::arrayValue)));
  if (params.size() != 0) {
    const int sub_id = dev::getUInt(params[0]);

    json_response["result"] = dev::toJS(subscriptions_.removeSubscription(sub_id));
  }

  return util::to_string(json_response);
}

std::shared_ptr<WsSession> JsonRpcWsServer::createSession(tcp::socket &&socket) {
  return std::make_shared<JsonRpcWsSession>(std::move(socket), node_addr_, shared_from_this());
}

}  // namespace taraxa::net
