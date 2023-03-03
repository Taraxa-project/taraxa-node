#pragma once

#include <jsonrpccpp/server/abstractserverconnector.h>

#include <algorithm>
#include <atomic>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "config/config.hpp"
#include "dag/dag_block.hpp"
#include "final_chain/data.hpp"
#include "pbft/pbft_chain.hpp"

namespace taraxa::net {

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

class WsServer;
class WsSession : public std::enable_shared_from_this<WsSession> {
 public:
  // Take ownership of the socket
  explicit WsSession(tcp::socket&& socket, addr_t node_addr, std::shared_ptr<WsServer> ws_server)
      : ws_(std::move(socket)) {
    LOG_OBJECTS_CREATE("WS_SESSION");
    ws_server_ = ws_server;
  }

  // Start the asynchronous operation
  void run();
  void close(bool normal = true);

  void on_accept(beast::error_code ec);
  void do_read();
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void on_write_no_read(beast::error_code ec, std::size_t bytes_transferred);

  virtual std::string processRequest(const std::string_view& request) = 0;

  void newEthBlock(const ::taraxa::final_chain::BlockHeader& payload, const TransactionHashes& trx_hashes);
  void newDagBlock(const DagBlock& blk);
  void newDagBlockFinalized(const blk_hash_t& blk, uint64_t period);
  void newPbftBlockExecuted(const Json::Value& payload);
  void newPendingTransaction(const trx_hash_t& trx_hash);
  bool is_closed() const { return closed_; }
  bool is_normal(const beast::error_code& ec) const;
  LOG_OBJECTS_DEFINE

 protected:
  void writeImpl(std::string&& message);
  void write();
  std::queue<std::string> queue_messages_;
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
  std::weak_ptr<WsServer> ws_server_;
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WsServer : public std::enable_shared_from_this<WsServer>, public jsonrpc::AbstractServerConnector {
 public:
  WsServer(boost::asio::io_context& ioc, tcp::endpoint endpoint, addr_t node_addr);
  virtual ~WsServer();

  WsServer(const WsServer&) = delete;
  WsServer(WsServer&&) = delete;
  WsServer& operator=(const WsServer&) = delete;
  WsServer& operator=(WsServer&&) = delete;

  // Start accepting incoming connections
  void run();
  void newEthBlock(const ::taraxa::final_chain::BlockHeader& payload, const TransactionHashes& trx_hashes);
  void newDagBlock(const DagBlock& blk);
  void newDagBlockFinalized(const blk_hash_t& blk, uint64_t period);
  void newPbftBlockExecuted(const PbftBlock& sche_blk, const std::vector<blk_hash_t>& finalized_dag_blk_hashes);
  void newPendingTransaction(const trx_hash_t& trx_hash);

  virtual std::shared_ptr<WsSession> createSession(tcp::socket&& socket) = 0;

  virtual bool StartListening() { return true; }
  virtual bool StopListening() { return true; }

 private:
  void do_accept();
  void on_accept(beast::error_code ec, tcp::socket socket);
  LOG_OBJECTS_DEFINE
  boost::asio::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::list<std::shared_ptr<WsSession>> sessions;
  std::atomic<bool> stopped_ = false;
  boost::shared_mutex sessions_mtx_;

 protected:
  const addr_t node_addr_;
};

}  // namespace taraxa::net
