#pragma once

#include <jsonrpccpp/server/abstractserverconnector.h>

#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <string>

#include "chain/final_chain.hpp"
#include "config/config.hpp"

namespace taraxa::net {

class HttpConnection;
class HttpHandler;

class HttpServer : public std::enable_shared_from_this<HttpServer>, public jsonrpc::AbstractServerConnector {
 public:
  HttpServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, addr_t node_addr,
             std::shared_ptr<FinalChain> final_chain = nullptr);
  virtual ~HttpServer() { HttpServer::StopListening(); }

  virtual bool StartListening() override;
  virtual bool StopListening() override;
  virtual bool SendResponse(const std::string &response, void *addInfo = NULL);
  void waitForAccept();
  boost::asio::io_context &getIoContext() { return io_context_; }
  std::shared_ptr<FinalChain> getFinalChain() { return final_chain_; }
  std::shared_ptr<HttpServer> getShared();
  virtual std::shared_ptr<HttpConnection> createConnection() = 0;
  friend HttpConnection;
  friend HttpHandler;

 protected:
  std::atomic<bool> stopped_ = true;
  boost::asio::io_context &io_context_;
  boost::asio::ip::tcp::endpoint ep_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::shared_ptr<FinalChain> final_chain_;
  LOG_OBJECTS_DEFINE;
};
// QQ:
// Why RpcConnection and Rpc use different io_context?
// Rpc use a io_context to create acceptor
// RpcConnection use the io_context from node to create socket

// node and rpc use same io_context

// QQ:
// atomic_flag responded, is RpcConnection multithreaded??

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
 public:
  explicit HttpConnection(std::shared_ptr<HttpServer> rpc);
  virtual ~HttpConnection() = default;
  virtual void read();
  virtual void write_response(std::string const &msg);
  virtual void write_options_response();
  boost::asio::ip::tcp::socket &getSocket() { return socket_; }
  virtual std::shared_ptr<HttpConnection> getShared();
  virtual std::string processRequest(std::string request) = 0;

 protected:
  std::shared_ptr<HttpServer> http_;
  boost::asio::ip::tcp::socket socket_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request<boost::beast::http::string_body> request_;
  boost::beast::http::response<boost::beast::http::string_body> response_;
  std::atomic_flag responded_;
};

}  // namespace taraxa::net
