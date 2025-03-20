#include "network/ws_server.hpp"

#include <json/value.h>
#include <json/writer.h>
#include <libdevcore/CommonJS.h>

#include <boost/beast/websocket/rfc6455.hpp>

#include "common/jsoncpp.hpp"
#include "network/rpc/eth/data.hpp"

namespace taraxa::net {

void WsSession::run() {
  // Set suggested timeout settings for the websocket
  ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

  // Enable permessage deflate for compression - DONT INCREASE ABOVE 14 it crashes node while running with indexer
  ws_.set_option(websocket::permessage_deflate{true, true, 14, 14});

  // Set a decorator to change the Server of the handshake
  ws_.set_option(websocket::stream_base::decorator([](websocket::response_type &res) {
    res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
  }));

  // Accept the websocket handshake
  ws_.async_accept(beast::bind_front_handler(&WsSession::on_accept, shared_from_this()));
}

void WsSession::on_accept(beast::error_code ec) {
  if (is_closed()) return;

  if (ec) {
    LOG(log_er_) << ec << " accept";
    return close();
  }

  // Read a message
  do_read();
}

void WsSession::do_read() {
  // Read a message into our buffer
  ws_.async_read(read_buffer_, beast::bind_front_handler(&WsSession::on_read, shared_from_this()));
}

void WsSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);
  if (is_closed()) return;

  // For any error close the connection
  if (ec) {
    LOG(log_nf_) << "WS closed in on_read " << ec;
    return close(is_normal(ec));
  }

  LOG(log_tr_) << "WS READ " << (static_cast<char *>(read_buffer_.data().data()));

  handleRequest();
  // Do another read
  do_read();
}

void WsSession::handleRequest() {
  if (is_closed()) return;

  std::string request(static_cast<char *>(read_buffer_.data().data()), read_buffer_.size());
  read_buffer_.consume(read_buffer_.size());
  LOG(log_tr_) << "processRequest " << request;

  auto executor = ws_.get_executor();
  if (!executor) {
    LOG(log_tr_) << "Executor missing - WS closed";
    close();
    return;
  }

  LOG(log_tr_) << "Before executor.post ";
  boost::asio::post(executor, [self = shared_from_this(), request = std::move(request)]() mutable {
    self->do_write(self->processRequest(request));
  });
  LOG(log_tr_) << "After executor.post ";
}

void WsSession::do_write(std::string &&message) {
  if (is_closed()) return;

  LOG(log_tr_) << "WS WRITE " << message.c_str();


  if (const auto executor = ws_.get_executor(); !executor) {
    LOG(log_tr_) << "Executor missing - WS closed";
    close();
    return;
  }

  LOG(log_tr_) << "Before async_write";
  boost::asio::post(write_strand_, [self = shared_from_this(), message = std::move(message)]() mutable {
    self->write(std::move(message));
  });
  LOG(log_tr_) << "After async_write";
}

void WsSession::write(std::string &&message) {
  if (is_closed()) return;

  try {
    ws_.text(true);  // as we are using text msg here
    ws_.write(boost::asio::buffer(std::move(message)));
  } catch (const boost::system::system_error &e) {
    // LOG(log_nf_) << "WS closed in on_write " << e.what();
    return close(is_normal(e.code()));
  }
  LOG(log_tr_) << "WS WRITE COMPLETE " << &ws_;
}

void WsSession::close(bool normal) {
  closed_ = true;
  if (ws_.is_open()) {
    ws_.async_close(normal ? beast::websocket::close_code::normal : beast::websocket::close_code::abnormal,
                    beast::bind_front_handler(&WsSession::on_close, shared_from_this()));
  }
}

void WsSession::on_close(beast::error_code ec) {
  if (ec) {
    LOG(log_er_) << "Error during async_close: " << ec;
  } else {
    LOG(log_tr_) << "Websocket closed successfully.";
  }
}

bool WsSession::is_normal(const beast::error_code &ec) {
  return ec == websocket::error::closed || ec == boost::asio::error::eof;
}

bool WsSession::is_closed() const { return closed_ || !ws_.is_open(); }

void WsSession::newEthBlock(const ::taraxa::final_chain::BlockHeader &payload, const TransactionHashes &trx_hashes) {
  if (new_heads_subscription_ != 0) {
    Json::Value res, params;
    res["jsonrpc"] = "2.0";
    res["method"] = "eth_subscription";
    params["result"] = rpc::eth::toJson(payload);
    params["result"]["transactions"] = rpc::eth::toJsonArray(trx_hashes);
    params["subscription"] = dev::toJS(new_heads_subscription_);
    res["params"] = params;
    auto response = util::to_string(res);
    LOG(log_tr_) << "WS WRITE " << response.c_str();
    do_write(std::move(response));
  }
}
void WsSession::newDagBlock(const std::shared_ptr<DagBlock> &blk) {
  if (new_dag_blocks_subscription_) {
    Json::Value res, params;
    res["jsonrpc"] = "2.0";
    res["method"] = "eth_subscription";
    params["result"] = blk->getJson();
    params["subscription"] = dev::toJS(new_dag_blocks_subscription_);
    res["params"] = params;
    auto response = util::to_string(res);
    auto executor = ws_.get_executor();
    if (!executor) {
      LOG(log_tr_) << "Executor missing - WS closed";
      return close(false);
    }
    do_write(std::move(response));
  }
}

void WsSession::newDagBlockFinalized(blk_hash_t const &blk, uint64_t period) {
  if (new_dag_block_finalized_subscription_) {
    Json::Value res, params, result;
    res["jsonrpc"] = "2.0";
    res["method"] = "eth_subscription";
    result["block"] = dev::toJS(blk);
    result["period"] = dev::toJS(period);
    params["result"] = result;
    params["subscription"] = dev::toJS(new_dag_block_finalized_subscription_);
    res["params"] = params;
    auto response = util::to_string(res);
    do_write(std::move(response));
  }
}

void WsSession::newPbftBlockExecuted(Json::Value const &payload) {
  if (new_pbft_block_executed_subscription_) {
    Json::Value res, params, result;
    res["jsonrpc"] = "2.0";
    res["method"] = "eth_subscription";
    result["pbft_block"] = payload;
    params["result"] = result;
    params["subscription"] = dev::toJS(new_pbft_block_executed_subscription_);
    res["params"] = params;
    auto response = util::to_string(res);
    do_write(std::move(response));
  }
}

void WsSession::newPillarBlockData(const pillar_chain::PillarBlockData &pillar_block_data) {
  if (new_pillar_block_subscription_) {
    Json::Value res, params;
    params["result"] = pillar_block_data.getJson(include_pillar_block_signatures);
    params["subscription"] = dev::toJS(new_pillar_block_subscription_);
    res["jsonrpc"] = "2.0";
    res["method"] = "eth_subscription";
    res["params"] = params;
    auto response = util::to_string(res);
    do_write(std::move(response));
  }
}

void WsSession::newPendingTransaction(trx_hash_t const &trx_hash) {
  if (new_transactions_subscription_) {
    Json::Value res, params;
    res["jsonrpc"] = "2.0";
    res["method"] = "eth_subscription";
    params["result"] = dev::toJS(trx_hash);
    params["subscription"] = dev::toJS(new_transactions_subscription_);
    res["params"] = params;
    auto response = util::to_string(res);
    do_write(std::move(response));
  }
}

WsServer::WsServer(boost::asio::io_context &ioc, tcp::endpoint endpoint, addr_t node_addr)
    : ioc_(ioc), acceptor_(ioc), node_addr_(std::move(node_addr)) {
  LOG_OBJECTS_CREATE("WS_SERVER");
  beast::error_code ec;

  // Open the acceptor
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    if (!stopped_) LOG(log_er_) << ec << " open";
    return;
  }

  // Allow address reuse
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    if (!stopped_) LOG(log_er_) << ec << " set_option";
    return;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    if (!stopped_) LOG(log_er_) << ec << " bind";
    return;
  }

  // Start listening for connections
  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    if (!stopped_) LOG(log_er_) << ec << " listen";
    return;
  }
  LOG(log_si_) << "Taraxa WS started at port: " << endpoint;
}

// Start accepting incoming connections
void WsServer::run() { do_accept(); }

WsServer::~WsServer() {
  stopped_ = true;
  ioc_.stop();
  acceptor_.close();
  boost::unique_lock<boost::shared_mutex> lock(sessions_mtx_);
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->close();
  }
  sessions.clear();
}

void WsServer::do_accept() {
  // The new connection gets its own strand
  acceptor_.async_accept(ioc_, beast::bind_front_handler(&WsServer::on_accept, shared_from_this()));
}

void WsServer::on_accept(beast::error_code ec, tcp::socket socket) {
  if (ec) {
    if (!stopped_) {
      LOG(log_er_) << ec << " Error on server accept, WS server down, check port";
      return;
    }
  } else {
    boost::unique_lock<boost::shared_mutex> lock(sessions_mtx_);
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
    sessions.push_back(createSession(std::move(socket)));
    sessions.back()->run();
  }

  // Accept another connection
  if (!stopped_) do_accept();
}

void WsServer::newDagBlock(const std::shared_ptr<DagBlock> &blk) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->newDagBlock(blk);
  }
}

void WsServer::newDagBlockFinalized(blk_hash_t const &blk, uint64_t period) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->newDagBlockFinalized(blk, period);
  }
}

void WsServer::newPbftBlockExecuted(PbftBlock const &pbft_blk,
                                    std::vector<blk_hash_t> const &finalized_dag_blk_hashes) {
  auto payload = PbftBlock::toJson(pbft_blk, finalized_dag_blk_hashes);
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->newPbftBlockExecuted(payload);
  }
}

void WsServer::newEthBlock(const ::taraxa::final_chain::BlockHeader &payload, const TransactionHashes &trx_hashes) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->newEthBlock(payload, trx_hashes);
  }
}

void WsServer::newPendingTransaction(trx_hash_t const &trx_hash) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->newPendingTransaction(trx_hash);
  }
}

void WsServer::newPillarBlockData(const pillar_chain::PillarBlockData &pillar_block_data) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  for (auto const &session : sessions) {
    if (!session->is_closed()) session->newPillarBlockData(pillar_block_data);
  }
}

uint32_t WsServer::numberOfSessions() {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  return sessions.size();
}

}  // namespace taraxa::net