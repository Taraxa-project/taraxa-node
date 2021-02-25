#pragma once

#include <jsonrpccpp/server/abstractserverconnector.h>

#include "network/http_processor.hpp"

namespace taraxa::net {

class JsonRpcHttpProcessor : public HttpProcessor, public jsonrpc::AbstractServerConnector {
 public:
  Response process(const Request& request) override;

  bool StartListening() override { return true; }
  bool StopListening() override { return true; }
  bool SendResponse(const std::string& response, void* addInfo = NULL) { return true; }
};

}  // namespace taraxa::net
