#include "WSServer.h"
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <libweb3jsonrpc/JsonHelper.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include "libdevcore/CommonJS.h"
#include "util.hpp"

namespace taraxa {

void WSSession::run() {
  // Set suggested timeout settings for the websocket
  ws_.set_option(
      websocket::stream_base::timeout::suggested(beast::role_type::server));

  // Set a decorator to change the Server of the handshake
  ws_.set_option(
      websocket::stream_base::decorator([](websocket::response_type &res) {
        res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) +
                                         " websocket-server-async");
      }));

  // Accept the websocket handshake
  ws_.async_accept(
      beast::bind_front_handler(&WSSession::on_accept, shared_from_this()));
}

void WSSession::on_accept(beast::error_code ec) {
  if (ec) {
    LOG(log_er_) << ec << " accept";
    return;
  }

  // Read a message
  do_read();
}

void WSSession::do_read() {
  // Read a message into our buffer
  ws_.async_read(buffer_, beast::bind_front_handler(&WSSession::on_read,
                                                    shared_from_this()));
}

void WSSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  // This indicates that the session was closed
  if (ec == websocket::error::closed) {
    LOG(log_tr_) << "WS closed";
    closed_ = true;
    return;
  }

  if (ec) LOG(log_er_) << ec << " read";

  LOG(log_tr_) << "WS READ " << ((char *)buffer_.data().data());

  Json::Value json;
  Json::Reader reader;
  bool parsingSuccessful =
      reader.parse((char *)buffer_.data().data(), json);  // parse process
  if (!parsingSuccessful) {
    LOG(log_er_) << "Failed to parse" << reader.getFormattedErrorMessages();
    return;
  }

  auto id = json.get("id", 0);
  Json::Value json_response;
  auto params = json.get("params", Json::Value(Json::Value(Json::arrayValue)));
  json_response["id"] = id;
  json_response["jsonrpc"] = "2.0";
  subscription_id_++;
  if (params.size() == 1 && params[0].asString() == "newHeads") {
    new_heads_subscription_ = subscription_id_;
  }
  json_response["result"] = dev::toJS(subscription_id_);
  Json::FastWriter fastWriter;
  std::string response = fastWriter.write(json_response);
  ws_.text(ws_.got_text());
  LOG(log_tr_) << "WS WRITE " << response.c_str();
  ws_.async_write(
      boost::asio::buffer(response),
      beast::bind_front_handler(&WSSession::on_write, shared_from_this()));
}

void WSSession::on_write(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    LOG(log_er_) << ec << " write";
    return;
  }
  // Clear the buffer
  buffer_.consume(buffer_.size());

  // Do another read
  do_read();
}

void WSSession::on_write_no_read(beast::error_code ec,
                                 std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    LOG(log_er_) << ec << " write";
    return;
  }
  // Clear the buffer
  buffer_.consume(buffer_.size());
}

void WSSession::newOrderedBlock(std::shared_ptr<taraxa::DagBlock> const &blk,
                                uint64_t const &block_number) {
  Json::Value res, params;
  res["jsonrpc"] = "2.0";
  res["method"] = "eth_subscription";
  params["result"] = dev::toJson(blk, block_number);
  params["subscription"] = dev::toJS(new_heads_subscription_);
  res["params"] = params;
  Json::FastWriter fastWriter;
  std::string response = fastWriter.write(res);
  ws_.text(ws_.got_text());
  LOG(log_tr_) << "WS WRITE " << response.c_str();
  ws_.async_write(boost::asio::buffer(response),
                  beast::bind_front_handler(&WSSession::on_write_no_read,
                                            shared_from_this()));
}

void WSSession::close() {
  closed_ = true;
  ws_.close("close");
}

WSServer::WSServer(net::io_context &ioc, tcp::endpoint endpoint)
    : ioc_(ioc), acceptor_(ioc) {
  beast::error_code ec;

  // Open the acceptor
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    LOG(log_er_) << ec << " open";
    return;
  }

  // Allow address reuse
  acceptor_.set_option(net::socket_base::reuse_address(true), ec);
  if (ec) {
    LOG(log_er_) << ec << " set_option";
    return;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    LOG(log_er_) << ec << " bind";
    return;
  }

  // Start listening for connections
  acceptor_.listen(net::socket_base::max_listen_connections, ec);
  if (ec) {
    LOG(log_er_) << ec << " listen";
    return;
  }
  LOG(log_si_) << "Taraxa WS started at port: " << endpoint << std::endl;
}

// Start accepting incoming connections
void WSServer::run() { do_accept(); }

void WSServer::stop() {
  stopped_ = true;
  acceptor_.close();
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->close();
  }
  sessions.clear();
}

void WSServer::do_accept() {
  // The new connection gets its own strand
  acceptor_.async_accept(
      net::make_strand(ioc_),
      beast::bind_front_handler(&WSServer::on_accept, shared_from_this()));
}

void WSServer::on_accept(beast::error_code ec, tcp::socket socket) {
  if (ec) {
    LOG(log_er_) << ec << " accept";
  } else {
    // Remove any close sessions
    auto session = sessions.begin();
    while (session != sessions.end()) {
      if ((*session)->is_closed()) {
        sessions.erase(session++);
      } else {
        session++;
      }
    }
    // Create the session and run it
    sessions.push_back(std::make_shared<WSSession>(std::move(socket)));
    sessions.back()->run();
  }

  // Accept another connection
  if (!stopped_) do_accept();
}

void WSServer::newOrderedBlock(std::shared_ptr<taraxa::DagBlock> const &blk,
                               uint64_t const &block_number) {
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->newOrderedBlock(blk, block_number);
  }
}

}  // namespace taraxa