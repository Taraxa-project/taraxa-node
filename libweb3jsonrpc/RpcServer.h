#ifndef TARAXA_NODE_LIBWEB3JSONRPC_RPC_HPP
#define TARAXA_NODE_LIBWEB3JSONRPC_RPC_HPP

#include <jsonrpccpp/server/abstractserverconnector.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include "config.hpp"
#include "libdevcore/Log.h"

namespace taraxa {

class RpcConnection;
class RpcHandler;

class RpcServer : public std::enable_shared_from_this<RpcServer>,
                  public jsonrpc::AbstractServerConnector {
 public:
  RpcServer(boost::asio::io_context &io, RpcConfig const &conf_rpc);
  virtual ~RpcServer() {
  // TODO
    if (!stopped_) StopListening();
  }

  virtual bool StartListening() override;
  virtual bool StopListening() override;
  virtual bool SendResponse(const std::string &response, void *addInfo = NULL);
  void waitForAccept();
  boost::asio::io_context &getIoContext() { return io_context_; }
  std::shared_ptr<RpcServer> getShared();
  friend RpcConnection;
  friend RpcHandler;

 private:
  bool stopped_ = true;
  RpcConfig conf_;
  boost::asio::io_context &io_context_;
  boost::asio::ip::tcp::acceptor acceptor_;
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "RPC")};
  dev::Logger log_er_{dev::createLogger(dev::Verbosity::VerbosityError, "RPC")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "RPC")};
  dev::Logger log_nf_{dev::createLogger(dev::Verbosity::VerbosityInfo, "RPC")};
  dev::Logger log_tr_{dev::createLogger(dev::Verbosity::VerbosityTrace, "RPC")};
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
  RpcConnection(std::shared_ptr<RpcServer> rpc);
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

}  // namespace taraxa
#endif