#include "http_server.hpp"

#include "consensus/pbft_chain.hpp"
#include "consensus/pbft_manager.hpp"
#include "consensus/vote.hpp"
#include "dag/dag_block.hpp"
#include "graphqlservice/GraphQLSchema.h"
#include "graphqlservice/GraphQLService.h"
#include "graphqlservice/JSONResponse.h"
#include "network/graphql/query.hpp"
#include "transaction_manager/transaction.hpp"
#include "util/util.hpp"

namespace taraxa::net {

HttpServer::HttpServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, addr_t node_addr,
                       std::shared_ptr<FinalChain> final_chain)
    : io_context_(io), acceptor_(io), ep_(std::move(ep)), final_chain_(final_chain) {
  LOG_OBJECTS_CREATE("RPC");
  LOG(log_si_) << "Taraxa RPC started at port: " << ep_.port();
}

std::shared_ptr<HttpServer> HttpServer::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "Rpc: " << e.what() << std::endl;
    assert(false);
  }
}

bool HttpServer::StartListening() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return true;
  }
  acceptor_.open(ep_.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

  boost::system::error_code ec;
  acceptor_.bind(ep_, ec);
  if (ec) {
    LOG(log_er_) << "RPC cannot bind ... " << ec.message() << "\n";
    throw std::runtime_error(ec.message());
  }
  acceptor_.listen();

  LOG(log_nf_) << "Rpc is listening on port " << ep_.port() << std::endl;
  waitForAccept();
  return true;
}

void HttpServer::waitForAccept() {
  std::shared_ptr<HttpConnection> connection = createConnection();
  acceptor_.async_accept(connection->getSocket(), [this, connection](boost::system::error_code const &ec) {
    if (!ec) {
      connection->read();
    } else {
      if (stopped_) return;

      LOG(log_er_) << "Error! Rpc async_accept error ... " << ec.message() << "\n";
      throw std::runtime_error(ec.message());
    }
    if (stopped_) return;
    waitForAccept();
  });
}

bool HttpServer::StopListening() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return true;
  }
  acceptor_.close();
  LOG(log_tr_) << "StopListening: ";
  return true;
}

bool HttpServer::SendResponse(const std::string &response, void *addInfo) { return true; }

std::shared_ptr<HttpConnection> HttpConnection::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "HttpConnection: " << e.what() << std::endl;
    return nullptr;
  }
}

HttpConnection::HttpConnection(std::shared_ptr<HttpServer> http) : http_(http), socket_(http->getIoContext()) {
  responded_.clear();
}

void HttpConnection::read() {
  auto this_sp = getShared();
  boost::beast::http::async_read(
      socket_, buffer_, request_, [this_sp](boost::system::error_code const &ec, size_t byte_transfered) {
        if (!ec) {
          // define response handler
          auto replier([this_sp](std::string const &msg) {
            // prepare response content
            std::string body = msg;
            this_sp->write_response(msg);
            // async write
            boost::beast::http::async_write(this_sp->socket_, this_sp->response_,
                                            [this_sp](boost::system::error_code const &ec, size_t byte_transfered) {});
          });
          if (this_sp->request_.method() == boost::beast::http::verb::options) {
            this_sp->write_options_response();
            // async write
            boost::beast::http::async_write(this_sp->socket_, this_sp->response_,
                                            [this_sp](boost::system::error_code const &ec, size_t byte_transfered) {});
          }
          if (this_sp->request_.method() == boost::beast::http::verb::post) {
            LOG(this_sp->http_->log_tr_) << "Read: " << this_sp->request_.body();
            string response = this_sp->processRequest(this_sp->request_.body());
            LOG(this_sp->http_->log_tr_) << "Write: " << response;
            replier(response);
          }
        } else {
          LOG(this_sp->http_->log_er_) << "Error! RPC conncetion read fail ... " << ec.message() << "\n";
        }
        (void)byte_transfered;
      });
}

void HttpConnection::write_response(std::string const &msg) {
  if (!responded_.test_and_set()) {
    response_.set("Content-Type", "application/json");
    response_.set("Access-Control-Allow-Origin", "*");
    response_.set("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
    response_.set("Connection", "close");
    response_.result(boost::beast::http::status::ok);
    response_.body() = msg;
    response_.prepare_payload();
  } else {
    assert(false && "RPC already responded ...\n");
  }
}

void HttpConnection::write_options_response() {
  if (!responded_.test_and_set()) {
    response_.set("Allow", "OPTIONS, GET, HEAD, POST");
    response_.set("Access-Control-Allow-Origin", "*");
    response_.set("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
    response_.set("Connection", "close");
    response_.result(boost::beast::http::status::no_content);
    response_.prepare_payload();
  } else {
    assert(false && "RPC already responded ...\n");
  }
}

}  // namespace taraxa::net
