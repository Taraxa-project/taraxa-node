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
   * @param api_ex_handler - if given, is a function that is called when an API method exception is caught.
   * It is supposed to re-throw the current exception internally (using `throw;`), catch it, and meaningfully convert
   * to `RpcServer::Error`. It is also allowed to not handle the original exception on re-throw - in which case
   * the default exception handling behavior will be applied by the caller.
   */
  RpcServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, addr_t node_addr,
            ApiExceptionHandler api_ex_handler = {});
  ~RpcServer() { RpcServer::StopListening(); }

  bool StartListening() override;
  bool StopListening() override;
  // TODO: error: only virtual member functions can be marked 'override'
  // After PR710, the compile error happens. Leave Oleh to fix
  // bool SendResponse(const std::string &response, void *addInfo = NULL) override;
  bool SendResponse(const std::string &response, void *addInfo = NULL);

 private:
  void accept();

  std::atomic<bool> stopped_ = true;
  boost::asio::io_context &io_context_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::ip::tcp::endpoint ep_;
  ApiExceptionHandler api_ex_handler_;

  LOG_OBJECTS_DEFINE
};

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
