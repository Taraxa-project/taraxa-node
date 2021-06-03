#include "RpcServer.h"

#include "util/jsoncpp.hpp"
#include "util/util.hpp"

namespace taraxa::net {

RpcServer::RpcServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, addr_t node_addr,
                     ApiExceptionHandler api_ex_handler)
    : io_context_(io), acceptor_(io), ep_(std::move(ep)), api_ex_handler_(std::move(api_ex_handler)) {
  LOG_OBJECTS_CREATE("RPC");
}

bool RpcServer::StartListening() {
  if (bool b = true; !stopped_.compare_exchange_strong(b, !b)) {
    return true;
  }
  LOG(log_si_) << "Taraxa RPC started at port: " << ep_.port();
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
  accept();
  return true;
}

void RpcServer::accept() {
  auto connection(make_shared<RpcConnection>(shared_from_this()));
  acceptor_.async_accept(connection->getSocket(), [this, connection](auto const &ec) {
    if (ec) {
      LOG(log_er_) << "Error! Rpc async_accept error ... " << ec.message() << "\n";
    } else {
      connection->read();
    }
    accept();
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

bool RpcServer::SendResponse(const std::string & /*response*/, void * /*addInfo*/) { return true; }

RpcConnection::RpcConnection(std::shared_ptr<RpcServer> rpc) : rpc_(move(rpc)), socket_(rpc_->io_context_) {}

void RpcConnection::read() {
  boost::beast::http::async_read(
      socket_, buffer_, request_, [this, this_sp = shared_from_this()](auto const &ec, auto /*bytes_transfered*/) {
        if (ec) {
          LOG(rpc_->log_er_) << "Error! RPC conncetion read fail ... " << ec.message() << "\n";
        } else {
          process_request();
        }
      });
}

void RpcConnection::process_request() {
  if (request_.method() == boost::beast::http::verb::options) {
    response_.set("Allow", "OPTIONS, POST");
    response_.result(boost::beast::http::status::no_content);
  } else if (request_.method() == boost::beast::http::verb::post) {
    LOG(rpc_->log_tr_) << "POST Read: " << request_.body();
    auto handler = rpc_->GetHandler();
    assert(handler);
    response_.set("Content-Type", "application/json");
    response_.result(boost::beast::http::status::ok);
    std::optional<RpcServer::Error> err;
    try {
      handler->HandleRequest(request_.body(), response_.body());
      LOG(rpc_->log_tr_) << "POST Write: " << response_.body();
    } catch (std::exception const &e) {
      err.emplace();
      if (rpc_->api_ex_handler_) {
        try {
          err = rpc_->api_ex_handler_();
        } catch (std::exception const &_e) {
          err->message << "Unhandled API exception: " << _e.what();
        }
      } else {
        err->message << "Uncaught API exception: " << e.what();
      }
    }
    if (err) {
      auto const &err_msg = err->message.str();
      if (err->code == jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR) {
        LOG(rpc_->log_er_) << "POST internal error. Request: " << request_.body() << ". Message: " << err_msg
                           << ". Extra data: " << err->data;
      }
      Json::Value res_json(Json::objectValue);
      res_json["jsonrpc"] = "2.0";
      auto req_json = util::parse_json(request_.body());
      if (req_json.isObject() && req_json.isMember("id") &&  // this conditional was taken from jsonrpccpp sources
          (req_json["id"].isNull() || req_json["id"].isIntegral() || req_json["id"].isString())) {
        res_json["id"] = req_json["id"];
      } else {
        res_json["id"] = Json::nullValue;
      }
      auto &res_json_error = res_json["error"] = Json::Value(Json::objectValue);
      res_json_error["code"] = err->code;
      res_json_error["message"] = err_msg;
      if (!err->data.empty()) {
        res_json_error["data"] = err->data;
      }
      response_.body() = util::to_string(res_json);
    }
  } else {
    response_.result(boost::beast::http::status::method_not_allowed);
  }
  response_.set("Access-Control-Allow-Origin", "*");
  response_.set("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
  response_.set("Connection", "close");
  response_.prepare_payload();
  boost::beast::http::async_write(socket_, response_,
                                  [this_sp = shared_from_this()](auto const & /*ec*/, auto /*bytes_transfered*/) {});
}

}  // namespace taraxa::net