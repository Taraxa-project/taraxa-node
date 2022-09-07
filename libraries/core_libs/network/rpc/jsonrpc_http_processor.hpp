#pragma once

#include <json/json.h>
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server/abstractserverconnector.h>

#include "network/http_server.hpp"

namespace taraxa::net {

class JsonRpcHttpProcessor final : public HttpProcessor, public jsonrpc::AbstractServerConnector {
 public:
  struct Error {
    int code = jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR;
    std::stringstream message;
    Json::Value data{Json::objectValue};
  };

  Response process(const Request& request) override;

  bool StartListening() override { return true; }
  bool StopListening() override { return true; }
};

}  // namespace taraxa::net