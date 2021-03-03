#include "jsonrpc_http_processor.hpp"

namespace taraxa::net {

HttpProcessor::Response JsonRpcHttpProcessor::process(const Request& request) {
  std::string response_str;

  if (GetHandler() != NULL) {
    GetHandler()->HandleRequest(request.body(), response_str);
  }

  return createOkResponse(std::move(response_str));
}

}  // namespace taraxa::net