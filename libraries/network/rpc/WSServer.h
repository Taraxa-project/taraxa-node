#pragma once

#include <jsonrpccpp/server/abstractserverconnector.h>

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

#include "config/config.hpp"
#include "pbft/chain.hpp"
#include "dag/dag_block.hpp"
#include "final_chain/data.hpp"

namespace taraxa::net {

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

class WSServer;
class WSSession : public std::enable_shared_from_this<WSSession> {
 public:
  // Take ownership of the socket
  explicit WSSession(tcp::socket&& socket, addr_t node_addr, std::shared_ptr<WSServer> ws_server)
      : ws_(std::move(socket)) {
    LOG_OBJECTS_CREATE("RPC");
    ws_server_ = ws_server;
  }

  // Start the asynchronous operation
  void run();
  void close();

  void on_accept(beast::error_code ec);
  void do_read();
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void on_write_no_read(beast::error_code ec, std::size_t bytes_transferred);
  void newEthBlock(::taraxa::final_chain::BlockHeader const& payload);
  void newDagBlock(DagBlock const& blk);
  void newDagBlockFinalized(blk_hash_t const& blk, uint64_t period);
  void newPbftBlockExecuted(Json::Value const& payload);
  void newPendingTransaction(trx_hash_t const& trx_hash);
  bool is_closed() { return closed_; }
  LOG_OBJECTS_DEFINE

 private:
  void writeImpl(const std::string& message);
  void write(const std::string& message);
  std::deque<std::string> queue_messages_;
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  std::string write_buffer_;
  int subscription_id_ = 0;
  int new_heads_subscription_ = 0;
  int new_dag_blocks_subscription_ = 0;
  int new_transactions_subscription_ = 0;
  int new_dag_block_finalized_subscription_ = 0;
  int new_pbft_block_executed_subscription_ = 0;
  std::atomic<bool> closed_ = false;
  std::weak_ptr<WSServer> ws_server_;
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WSServer : public std::enable_shared_from_this<WSServer>, public jsonrpc::AbstractServerConnector {
 public:
  WSServer(boost::asio::io_context& ioc, tcp::endpoint endpoint, addr_t node_addr);
  ~WSServer();

  // Start accepting incoming connections
  void run();
  void newEthBlock(::taraxa::final_chain::BlockHeader const& payload);
  void newDagBlock(DagBlock const& blk);
  void newDagBlockFinalized(blk_hash_t const& blk, uint64_t period);
  void newPbftBlockExecuted(PbftBlock const& sche_blk, std::vector<blk_hash_t> const& finalized_dag_blk_hashes);
  void newPendingTransaction(trx_hash_t const& trx_hash);

  virtual bool StartListening() { return true; };
  virtual bool StopListening() { return true; };
  virtual bool SendResponse(const std::string& /*response*/, void* /*addInfo*/) { return true; };

 private:
  void do_accept();
  void on_accept(beast::error_code ec, tcp::socket socket);
  LOG_OBJECTS_DEFINE
  boost::asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::list<std::shared_ptr<WSSession>> sessions;
  std::atomic<bool> stopped_ = false;
  boost::shared_mutex sessions_mtx_;
  addr_t node_addr_;
};

}  // namespace taraxa::net
