#ifndef TARAXA_NODE_WSSERVER_HPP
#define TARAXA_NODE_WSSERVER_HPP

#include <algorithm>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "dag_block.hpp"
#include "libdevcore/Log.h"

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

namespace taraxa {

class WSSession : public std::enable_shared_from_this<WSSession> {
 public:
  // Take ownership of the socket
  explicit WSSession(tcp::socket&& socket) : ws_(std::move(socket)) {}

  // Start the asynchronous operation
  void run();
  void close();

  void on_accept(beast::error_code ec);
  void do_read();
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void on_write(beast::error_code ec, std::size_t bytes_transferred);
  void on_write_no_read(beast::error_code ec, std::size_t bytes_transferred);
  void newOrderedBlock(std::shared_ptr<taraxa::DagBlock> const& blk,
                       uint64_t const& block_number);
  void newPendingTransaction(trx_hash_t const &trx_hash);
  bool is_closed() { return closed_; }
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "RPC")};
  dev::Logger log_er_{dev::createLogger(dev::Verbosity::VerbosityError, "RPC")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "RPC")};
  dev::Logger log_nf_{dev::createLogger(dev::Verbosity::VerbosityInfo, "RPC")};
  dev::Logger log_tr_{dev::createLogger(dev::Verbosity::VerbosityTrace, "RPC")};

 private:
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  int subscription_id_ = 0;
  int new_heads_subscription_ = 0;
  int new_transactions_subscription_ = 0;
  bool closed_ = false;
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WSServer : public std::enable_shared_from_this<WSServer> {
 public:
  WSServer(net::io_context& ioc, tcp::endpoint endpoint);

  // Start accepting incoming connections
  void run();
  void stop();
  void newOrderedBlock(std::shared_ptr<taraxa::DagBlock> const& blk,
                       uint64_t const& block_number);
  void newPendingTransaction(trx_hash_t const &trx_hash);

 private:
  void do_accept();
  void on_accept(beast::error_code ec, tcp::socket socket);
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "RPC")};
  dev::Logger log_er_{dev::createLogger(dev::Verbosity::VerbosityError, "RPC")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "RPC")};
  dev::Logger log_nf_{dev::createLogger(dev::Verbosity::VerbosityInfo, "RPC")};
  dev::Logger log_tr_{dev::createLogger(dev::Verbosity::VerbosityTrace, "RPC")};

  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::list<std::shared_ptr<WSSession>> sessions;
  bool stopped_ = false;
};
}  // namespace taraxa

#endif