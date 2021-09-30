#include "rpc_error_handler.hpp"

#include <json/json.h>

#include "final_chain/state_api.hpp"

namespace taraxa::net {

RpcServer::Error handle_rpc_error() {
  RpcServer::Error err;
  try {
    throw;
  } catch (state_api::ErrFutureBlock const& ex) {
    err.code = jsonrpc::Errors::ERROR_RPC_INVALID_PARAMS;
    err.message << ex.what();
  }
  return err;
}

}  // namespace taraxa::net