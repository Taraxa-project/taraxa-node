#ifndef TARAXA_NODE_NET_RPC_SEERVER_H_
#define TARAXA_NODE_NET_RPC_SEERVER_H_

#include <jsonrpccpp/server/abstractserverconnector.h>

#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <string>

#include "config.hpp"

namespace taraxa::net {

class RpcConnection;
class RpcHandler;

class RpcServer : public std::enable_shared_from_this<RpcServer>, public jsonrpc::AbstractServerConnector {
 public:
  RpcServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, addr_t node_addr);
  virtual ~RpcServer() { RpcServer::StopListening(); }

  virtual bool StartListening() override;
  virtual bool StopListening() override;
  virtual bool SendResponse(const std::string &response, void *addInfo = NULL);
  void waitForAccept();
  boost::asio::io_context &getIoContext() { return io_context_; }
  std::shared_ptr<RpcServer> getShared();
  friend RpcConnection;
  friend RpcHandler;

 private:
  std::atomic<bool> stopped_ = true;
  boost::asio::io_context &io_context_;
  boost::asio::ip::tcp::endpoint ep_;
  boost::asio::ip::tcp::acceptor acceptor_;
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
  virtual ~RpcConnection() = default;
  virtual void read();
  virtual void write_response(std::string const &msg);
  virtual void write_options_response();
  boost::asio::ip::tcp::socket &getSocket() { return socket_; }
  virtual std::shared_ptr<RpcConnection> getShared();

 private:
  std::shared_ptr<RpcServer> rpc_;
  boost::asio::ip::tcp::socket socket_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request<boost::beast::http::string_body> request_;
  boost::beast::http::response<boost::beast::http::string_body> response_;
  std::atomic_flag responded_;
};

}  // namespace taraxa::net

#endif