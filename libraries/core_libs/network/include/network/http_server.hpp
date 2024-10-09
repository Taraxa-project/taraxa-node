#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "common/thread_pool.hpp"
#include "common/types.hpp"
#include "logger/logger.hpp"

namespace taraxa::net {

class HttpProcessor {
 public:
  using Request = boost::beast::http::request<boost::beast::http::string_body>;
  using Response = boost::beast::http::response<boost::beast::http::string_body>;

  virtual Response process(const Request& request) = 0;
};

class HttpConnection;
class HttpHandler;

class HttpServer : public std::enable_shared_from_this<HttpServer> {
 public:
  HttpServer(std::shared_ptr<util::ThreadPool> thread_pool, boost::asio::ip::tcp::endpoint ep, const addr_t& node_addr,
             const std::shared_ptr<HttpProcessor>& request_processor, uint32_t max_pending_tasks);

  virtual ~HttpServer() { HttpServer::stop(); }

  bool start();
  bool stop();

  void accept();
  uint32_t numberOfPendingTasks() const;
  bool pendingTasksOverLimit() const { return numberOfPendingTasks() > kMaxPendingTasks; }
  boost::asio::io_context& getIoContext() { return io_context_; }
  std::shared_ptr<HttpServer> getShared();
  std::shared_ptr<HttpConnection> createConnection();
  friend HttpConnection;
  friend HttpHandler;

 protected:
  std::shared_ptr<HttpProcessor> request_processor_;

 private:
  std::atomic<bool> stopped_ = true;
  boost::asio::io_context& io_context_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::ip::tcp::endpoint ep_;
  std::weak_ptr<util::ThreadPool> thread_pool_;
  const uint32_t kMaxPendingTasks;
  LOG_OBJECTS_DEFINE
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
  explicit HttpConnection(const std::shared_ptr<HttpServer>& http_server);
  virtual ~HttpConnection() { HttpConnection::stop(); }
  boost::asio::ip::tcp::socket& getSocket() { return socket_; }
  virtual std::shared_ptr<HttpConnection> getShared();
  void read();
  void stop();

 protected:
  std::shared_ptr<HttpServer> server_;
  boost::asio::ip::tcp::socket socket_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request<boost::beast::http::string_body> request_;
  boost::beast::http::response<boost::beast::http::string_body> response_;
};

}  // namespace taraxa::net