#include "RpcServer.h"

#include "dag_block.hpp"
#include "pbft_chain.hpp"
#include "pbft_manager.hpp"
#include "transaction.hpp"
#include "util.hpp"
#include "vote.hpp"

namespace taraxa::net {

RpcServer::RpcServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, addr_t node_addr)
    : io_context_(io), acceptor_(io), ep_(std::move(ep)) {
  LOG_OBJECTS_CREATE("RPC");
  LOG(log_si_) << "Taraxa RPC started at port: " << ep_.port();
}

std::shared_ptr<RpcServer> RpcServer::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "Rpc: " << e.what() << std::endl;
    assert(false);
  }
}

bool RpcServer::StartListening() {
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

void RpcServer::waitForAccept() {
  std::shared_ptr<RpcConnection> connection(std::make_shared<RpcConnection>(getShared()));
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

bool RpcServer::StopListening() {
  if (bool b = false; !stopped_.compare_exchange_strong(b, !b)) {
    return true;
  }
  acceptor_.close();
  LOG(log_tr_) << "StopListening: ";
  return true;
}

bool RpcServer::SendResponse(const std::string &response, void *addInfo) { return true; }

std::shared_ptr<RpcConnection> RpcConnection::getShared() {
  try {
    return shared_from_this();
  } catch (std::bad_weak_ptr &e) {
    std::cerr << "RpcConnection: " << e.what() << std::endl;
    return nullptr;
  }
}

RpcConnection::RpcConnection(std::shared_ptr<RpcServer> rpc) : rpc_(rpc), socket_(rpc->getIoContext()) { responded_.clear(); }

void RpcConnection::read() {
  auto this_sp = getShared();
  boost::beast::http::async_read(socket_, buffer_, request_, [this_sp](boost::system::error_code const &ec, size_t byte_transfered) {
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
        string response;
        if (this_sp->rpc_->GetHandler() != NULL) {
          LOG(this_sp->rpc_->log_tr_) << "Read: " << this_sp->request_.body();
          this_sp->rpc_->GetHandler()->HandleRequest(this_sp->request_.body(), response);
        }
        LOG(this_sp->rpc_->log_tr_) << "Write: " << response;
        replier(response);
      }
    } else {
      LOG(this_sp->rpc_->log_er_) << "Error! RPC conncetion read fail ... " << ec.message() << "\n";
    }
    (void)byte_transfered;
  });
}

void RpcConnection::write_response(std::string const &msg) {
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

void RpcConnection::write_options_response() {
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