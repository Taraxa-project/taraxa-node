#include "network/http_server.hpp"

namespace taraxa::net {

HttpServer::HttpServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, const addr_t &node_addr,
                       const std::shared_ptr<HttpProcessor> &request_processor,
                       std::shared_ptr<metrics::JsonRpcMetrics> metrics)
    : request_processor_(request_processor), metrics_(metrics), io_context_(io), acceptor_(io), ep_(std::move(ep)) {
  LOG_OBJECTS_CREATE("HTTP");
  LOG(log_si_) << "Taraxa HttpServer started at port: " << ep_.port();
}

std::shared_ptr<HttpServer> HttpServer::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "HttpServer: " << e.what() << std::endl;
    assert(false);
  }
}

std::shared_ptr<HttpConnection> HttpServer::createConnection() { return std::make_shared<HttpConnection>(getShared()); }

bool HttpServer::start() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return true;
  }
  acceptor_.open(ep_.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

  boost::system::error_code ec;
  acceptor_.bind(ep_, ec);
  if (ec) {
    LOG(log_er_) << "HttpServer cannot bind ... " << ec.message() << "\n";
    throw std::runtime_error(ec.message());
  }
  acceptor_.listen();

  LOG(log_nf_) << "HttpServer is listening on port " << ep_.port() << std::endl;
  accept();
  return true;
}

void HttpServer::accept() {
  std::shared_ptr<HttpConnection> connection = createConnection();
  acceptor_.async_accept(connection->getSocket(), [this, connection](boost::system::error_code const &ec) {
    if (!ec) {
      connection->read();
    } else {
      if (stopped_) return;

      LOG(log_er_) << "Error! HttpServer async_accept error ... " << ec.message() << "\n";
      throw std::runtime_error(ec.message());
    }
    if (stopped_) return;
    accept();
  });
}

bool HttpServer::stop() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return true;
  }
  io_context_.stop();
  acceptor_.close();
  LOG(log_tr_) << "stop";
  return true;
}

std::shared_ptr<HttpConnection> HttpConnection::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "HttpConnection: " << e.what() << std::endl;
    assert(false);
  }
}

HttpConnection::HttpConnection(const std::shared_ptr<HttpServer> &http_server)
    : server_(http_server), socket_(http_server->getIoContext()) {}

void HttpConnection::stop() {
  if (socket_.is_open()) {
    try {
      LOG(server_->log_dg_) << "Closing connection...";
      socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
      socket_.close();
    } catch (...) {
    }
  }
}

void HttpConnection::read() {
  boost::beast::http::async_read(
      socket_, buffer_, request_, [this, this_sp = getShared()](boost::system::error_code const &ec, size_t) {
        if (ec) {
          LOG(server_->log_er_) << "Error! HttpConnection connection read fail ... " << ec.message() << std::endl;
          stop();
        } else {
          std::string ip = request_["X-Real-IP"];
          if (ip.empty()) {
            try {
              auto endpoint = socket_.remote_endpoint();
              ip = endpoint.address().to_string();
            } catch (...) {
              ip = "Unknown";
            }
          }

          assert(server_->request_processor_);
          LOG(server_->log_dg_) << "Received: " << request_;

          auto start_time = std::chrono::steady_clock::now();
          response_ = server_->request_processor_->process(request_);
          auto end_time = std::chrono::steady_clock::now();
          auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

          if (server_->metrics_) {
            server_->metrics_->report(request_.body(), ip, "HTTP", processing_time.count());
          }

          boost::beast::http::async_write(
              socket_, response_,
              [this_sp = getShared()](auto const & /*ec*/, auto /*bytes_transferred*/) { this_sp->stop(); });
        }
      });
}

}  // namespace taraxa::net