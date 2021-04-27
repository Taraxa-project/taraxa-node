#pragma once

#include <json/json.h>
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server/abstractserverconnector.h>

#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <string>

#include "config/config.hpp"

namespace taraxa::net {

class RpcConnection;

struct RpcServer final : std::enable_shared_from_this<RpcServer>, jsonrpc::AbstractServerConnector {
  friend RpcConnection;

  struct Error {
    int code = jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR;
    stringstream message;
    Json::Value data{Json::objectValue};
  };

  using ApiExceptionHandler = std::function<Error()>;

  /**
   * @param api_error_handler - if given, is a function that is called when an API method exception is caught.
   * It is supposed to re-throw the current exception internally (using `throw;`), catch it, and meaningfully convert
   * to APIError. It is also allowed to not handle the original exception on re-throw - in which case the default
   * exception handling behavior will be applied by the caller.
   */
  RpcServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, addr_t node_addr,
            ApiExceptionHandler api_ex_handler = {});
  ~RpcServer() { RpcServer::StopListening(); }

  bool StartListening() override;
  bool StopListening() override;
  bool SendResponse(const std::string &response, void *addInfo = NULL) override;

 private:
  void accept();

  std::atomic<bool> stopped_ = true;
  boost::asio::io_context &io_context_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::ip::tcp::endpoint ep_;
  ApiExceptionHandler api_ex_handler_;
  LOG_OBJECTS_DEFINE;
};
// QQ:
// Why RpcConnection and Rpc use different io_context?
// Rpc use a io_context to create acceptor
// RpcConnection use the io_context from node to create socket

// node and rpc use same io_context

// QQ:
// atomic_flag responded, is RpcConnection multithreaded??

class RpcConnection : public std::enable_shared_from_this<RpcConnection> {
 public:
  explicit RpcConnection(std::shared_ptr<RpcServer> rpc);
  void read();
  boost::asio::ip::tcp::socket &getSocket() { return socket_; }

 private:
  void process_request();

  std::shared_ptr<RpcServer> rpc_;
  boost::asio::ip::tcp::socket socket_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request<boost::beast::http::string_body> request_;
  boost::beast::http::response<boost::beast::http::string_body> response_;
};

}  // namespace taraxa::net
