/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-28 16:05:18
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-23 18:22:52
 */

#ifndef RPC_HPP
#define RPC_HPP
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include "config.hpp"
#include "libdevcore/Log.h"
#include "rpc.hpp"

// TODO:
// Need a buffer for rpc requests ...

namespace taraxa {

class FullNode;
class RpcConnection;
class RpcHandler;

class Rpc : public std::enable_shared_from_this<Rpc> {
 public:
  Rpc(boost::asio::io_context &io, RpcConfig const &conf_rpc,
      std::shared_ptr<FullNode> node);
  virtual ~Rpc() {
    if (!stopped_) stop();
  }
  void start();
  void waitForAccept();
  void stop();
  boost::asio::io_context &getIoContext() { return io_context_; }
  std::shared_ptr<Rpc> getShared();
  friend RpcConnection;
  friend RpcHandler;

 private:
  bool stopped_ = true;
  RpcConfig conf_;
  boost::asio::io_context &io_context_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::shared_ptr<FullNode> node_;
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
  RpcConnection(std::shared_ptr<Rpc> rpc, std::shared_ptr<FullNode> node);
  virtual ~RpcConnection() = default;
  virtual void read();
  virtual void write_response(std::string const &msg);
  boost::asio::ip::tcp::socket &getSocket() { return socket_; }
  virtual std::shared_ptr<RpcConnection> getShared();

 private:
  std::shared_ptr<Rpc> rpc_;
  std::shared_ptr<FullNode> node_;
  boost::asio::ip::tcp::socket socket_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request<boost::beast::http::string_body> request_;
  boost::beast::http::response<boost::beast::http::string_body> response_;
  std::atomic_flag responded_;
};

class RpcHandler : public std::enable_shared_from_this<RpcHandler> {
 public:
  RpcHandler(
      std::shared_ptr<Rpc> rpc, std::shared_ptr<FullNode> node,
      std::string const &body,
      std::function<void(std::string const &msg)> const &response_handler);
  void processRequest();
  std::shared_ptr<RpcHandler> getShared();

 private:
  bool accountCreate();
  std::shared_ptr<Rpc> rpc_;
  std::shared_ptr<FullNode> node_;
  std::string body_;
  boost::property_tree::ptree in_doc_;
  std::function<void(std::string const &msg)>
      replier_;  // Capture RpcConnector and reply from there
};

}  // namespace taraxa
#endif
