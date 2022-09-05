#include "network/http_server.hpp"

namespace taraxa::net {

HttpServer::HttpServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, const addr_t &node_addr,
                       const std::shared_ptr<HttpProcessor> &request_processor)
    : request_processor_(request_processor), io_context_(io), acceptor_(io), ep_(std::move(ep)) {
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

void HttpConnection::read() {
  boost::beast::http::async_read(
      socket_, buffer_, request_, [this, this_sp = getShared()](boost::system::error_code const &ec, size_t) {
        if (ec) {
          LOG(server_->log_er_) << "Error! HttpConnection connection read fail ... " << ec.message() << "\n";
        } else {
          assert(server_->request_processor_);
          response_ = server_->request_processor_->process(request_);
          boost::beast::http::async_write(socket_, response_,
                                          [this_sp = getShared()](auto const & /*ec*/, auto /*bytes_transfered*/) {});
        }
      });
}

}  // namespace taraxa::net