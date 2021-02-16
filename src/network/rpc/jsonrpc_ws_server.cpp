#include "jsonrpc_ws_server.hpp"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <libdevcore/CommonJS.h>
#include <libweb3jsonrpc/JsonHelper.h>

#include "config/config.hpp"
#include "util/util.hpp"

namespace taraxa::net {

std::string JsonRpcWSSession::processRequest(std::string request) {
  Json::Value json;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(request.c_str(), json);  // parse process
  if (!parsingSuccessful) {
    LOG(log_er_) << "Failed to parse" << reader.getFormattedErrorMessages();
    closed_ = true;
    return "";
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
    Json::FastWriter fastWriter;
    response = fastWriter.write(json_response);
    ws_.text(ws_.got_text());
    LOG(log_tr_) << "WS WRITE " << response.c_str();
  } else {
    auto ws_server = ws_server_.lock();
    if (ws_server) {
      auto handler = ws_server->GetHandler();
      if (handler != NULL) {
        LOG(log_tr_) << "WS Read: " << (char *)buffer_.data().data();
        handler->HandleRequest((char *)buffer_.data().data(), response);
      }
      LOG(log_tr_) << "WS Write: " << response;
    }
  }
  auto executor = ws_.get_executor();
  if (!executor) {
    LOG(log_tr_) << "Executor missing - WS closed";
    closed_ = true;
    return "";
  }

  return response;
}

std::shared_ptr<WSSession> JsonRpcWsServer::createSession(tcp::socket &socket) {
  return std::make_shared<JsonRpcWSSession>(std::move(socket), node_addr_, shared_from_this());
}

}  // namespace taraxa::net