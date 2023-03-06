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

  auto id = json.get("id", 0);
  Json::Value json_response;
  auto method = json.get("method", "");
  std::string response;
  if (method == "eth_subscribe") {
    auto params = json.get("params", Json::Value(Json::Value(Json::arrayValue)));
    json_response["id"] = id;
    json_response["jsonrpc"] = "2.0";
    subscription_id_++;
    if (params.size() > 0) {
      if (params[0].asString() == "newHeads") {
        new_heads_subscription_ = subscription_id_;
      } else if (params[0].asString() == "newPendingTransactions") {
        new_transactions_subscription_ = subscription_id_;
      } else if (params[0].asString() == "newDagBlocks") {
        new_dag_blocks_subscription_ = subscription_id_;
      } else if (params[0].asString() == "newDagBlocksFinalized") {
        new_dag_block_finalized_subscription_ = subscription_id_;
      } else if (params[0].asString() == "newPbftBlocks") {
        new_pbft_block_executed_subscription_ = subscription_id_;
      }
    }
    json_response["result"] = dev::toJS(subscription_id_);
    response = util::to_string(json_response);
    LOG(log_tr_) << "WS WRITE " << response.c_str();
  } else {
    auto ws_server = ws_server_.lock();
    if (ws_server) {
      auto handler = ws_server->GetHandler();
      if (handler != NULL) {
        try {
          LOG(log_tr_) << "WS Read: " << static_cast<char *>(buffer_.data().data());
          handler->HandleRequest(static_cast<char *>(buffer_.data().data()), response);
        } catch (std::exception const &e) {
          LOG(log_er_) << "Exception " << e.what();
          auto &res_json_error = json_response["error"] = Json::Value(Json::objectValue);
          res_json_error["code"] = jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR;
          res_json_error["message"] = e.what();
          json_response["id"] = id;
          json_response["jsonrpc"] = "2.0";
          response = util::to_string(json_response);
        }
        LOG(log_tr_) << "WS Write: " << response;
      }
    }
  }
  return response;
}

std::shared_ptr<WsSession> JsonRpcWsServer::createSession(tcp::socket &&socket) {
  return std::make_shared<JsonRpcWsSession>(std::move(socket), node_addr_, shared_from_this());
}

}  // namespace taraxa::net