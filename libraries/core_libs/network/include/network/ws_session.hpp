#pragma once

#include <json/value.h>
#include <jsonrpccpp/server/abstractserverconnector.h>

#include <atomic>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <memory>
#include <string>

#include "common/types.hpp"
#include "final_chain/data.hpp"
#include "logger/logger.hpp"
#include "network/subscriptions.hpp"

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

namespace taraxa::net {

class WsServer;
class WsSession : public std::enable_shared_from_this<WsSession> {
 public:
  // Take ownership of the socket
  explicit WsSession(tcp::socket&& socket, addr_t node_addr, std::shared_ptr<WsServer> ws_server)
      : ws_(std::move(socket)),
        ws_server_(ws_server),
        subscriptions_(std::bind(&WsSession::do_write, this, std::placeholders::_1)),
        write_strand_(boost::asio::make_strand(ws_.get_executor())) {
    LOG_OBJECTS_CREATE("WS_SESSION");
  }

  // Start the asynchronous operation
  void run();
  void close(bool normal = true);
  bool is_closed() const;

  virtual std::string processRequest(const std::string_view& request) = 0;

  void newEthBlock(const Json::Value& payload);
  void newDagBlock(const Json::Value& blk);
  void newDagBlockFinalized(const Json::Value& payload);
  void newPbftBlockExecuted(const Json::Value& payload);
  void newPendingTransaction(const Json::Value& payload);
  void newPillarBlockData(const Json::Value& payload);
  void newLogs(const final_chain::BlockHeader& header, TransactionHashes trx_hashes,
               const TransactionReceipts& receipts);

  LOG_OBJECTS_DEFINE
 private:
  static bool is_normal(const beast::error_code& ec);
  void on_close(beast::error_code ec);
  void on_accept(beast::error_code ec);
  void do_read();
  void on_read(beast::error_code ec, std::size_t bytes_transferred);
  void write(std::string&& message);

 protected:
  void handleRequest();
  void do_write(std::string&& message);

  websocket::stream<beast::tcp_stream> ws_;
  std::atomic<int> subscription_id_ = 0;
  std::weak_ptr<WsServer> ws_server_;
  Subscriptions subscriptions_;

 private:
  boost::asio::strand<boost::asio::any_io_executor> write_strand_;
  beast::flat_buffer read_buffer_;
  std::atomic<bool> closed_ = false;
  std::string ip_;
};
}  // namespace taraxa::net