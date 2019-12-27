#ifndef TARAXA_NODE_NET_WS_SERVER_H_
#define TARAXA_NODE_NET_WS_SERVER_H_

#include <libdevcore/Log.h>

#include <algorithm>
#include <atomic>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "dag_block.hpp"
#include "pbft_chain.hpp"

namespace taraxa::net {

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

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
  void newOrderedBlock(Json::Value const& payload);
  void newDagBlock(DagBlock const& blk);
  void newDagBlockFinalized(blk_hash_t const& blk, uint64_t period);
  void newScheduleBlockExecuted(ScheduleBlock const& sche_blk,
                                uint32_t block_number, uint32_t period);
  void newPendingTransaction(trx_hash_t const& trx_hash);
  bool is_closed() { return closed_; }
  dev::Logger log_si_{
      dev::createLogger(dev::Verbosity::VerbositySilent, "RPC")};
  dev::Logger log_er_{dev::createLogger(dev::Verbosity::VerbosityError, "RPC")};
  dev::Logger log_wr_{
      dev::createLogger(dev::Verbosity::VerbosityWarning, "RPC")};
  dev::Logger log_nf_{dev::createLogger(dev::Verbosity::VerbosityInfo, "RPC")};
  dev::Logger log_tr_{dev::createLogger(dev::Verbosity::VerbosityTrace, "RPC")};

 private:
  void writeImpl(const std::string& message);
  void write(const std::string& message);
  std::deque<std::string> queue_messages_;
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  int subscription_id_ = 0;
  int new_heads_subscription_ = 0;
  int new_dag_blocks_subscription_ = 0;
  int new_transactions_subscription_ = 0;
  int new_dag_block_finalized_subscription_ = 0;
  int new_schedule_block_executed_subscription_ = 0;
  std::atomic<bool> closed_ = false;
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WSServer : public std::enable_shared_from_this<WSServer> {
 public:
  WSServer(boost::asio::io_context& ioc, tcp::endpoint endpoint);
  ~WSServer();

  // Start accepting incoming connections
  void run();
  void newOrderedBlock(Json::Value const& payload);
  void newDagBlock(DagBlock const& blk);
  void newDagBlockFinalized(blk_hash_t const& blk, uint64_t period);
  void newScheduleBlockExecuted(ScheduleBlock const& sche_blk,
                                uint32_t block_number, uint32_t period);
  void newPendingTransaction(trx_hash_t const& trx_hash);

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

  boost::asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::list<std::shared_ptr<WSSession>> sessions;
  std::atomic<bool> stopped_ = false;
  boost::shared_mutex sessions_mtx_;
};

}  // namespace taraxa::net

#endif