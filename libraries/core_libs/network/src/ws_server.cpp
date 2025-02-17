#include "network/ws_server.hpp"

#include <json/value.h>
#include <json/writer.h>
#include <libdevcore/CommonJS.h>

#include <boost/beast/websocket/rfc6455.hpp>

#include "network/rpc/eth/data.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::net {

void WsSession::run() {
  // Set suggested timeout settings for the websocket
  ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

  // Set a decorator to change the Server of the handshake
  ws_.set_option(websocket::stream_base::decorator([](websocket::response_type &res) {
    res.set(beast::http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
  }));

  // Accept the websocket handshake
  ws_.async_accept(beast::bind_front_handler(&WsSession::on_accept, shared_from_this()));
}

void WsSession::on_accept(beast::error_code ec) {
  if (ec) {
    if (!closed_) LOG(log_er_) << ec << " accept";
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

  auto ws_server = ws_server_.lock();
  if (ws_server && ws_server->pendingTasksOverLimit()) {
    LOG(log_er_) << "WS closed - pending tasks over the limit " << ws_server->numberOfPendingTasks();
    return close(true);
  }

  processAsync();
  // Do another read
  do_read();
}

void WsSession::processAsync() {
  if (closed_) return;

  std::string request(static_cast<char *>(read_buffer_.data().data()), read_buffer_.size());
  read_buffer_.consume(read_buffer_.size());
  LOG(log_tr_) << "processAsync " << request;
  auto executor = ws_.get_executor();
  if (!executor) {
    LOG(log_tr_) << "Executor missing - WS closed";
    closed_ = true;
    return;
  }

  boost::asio::post(executor, [this, request = std::move(request)]() mutable { writeAsync(processRequest(request)); });
}

void WsSession::writeAsync(std::string &&message) {
  if (closed_) return;

  auto executor = ws_.get_executor();
  if (!executor) {
    LOG(log_tr_) << "Executor missing - WS closed";
    closed_ = true;
    return;
  }

  boost::asio::post(write_strand_, [this, message = std::move(message)]() mutable { writeImpl(std::move(message)); });
}

void WsSession::writeImpl(std::string &&message) {
  ws_.text(true);  // as we are using text msg here
  try {
    ws_.write(boost::asio::buffer(message));
  } catch (const boost::system::system_error &e) {
    LOG(log_nf_) << "WS closed in on_write " << e.what();
    return close(is_normal(e.code()));
  }
}

void WsSession::newEthBlock(const Json::Value &payload) { subscriptions_.process(SubscriptionType::HEADS, payload); }
void WsSession::newDagBlock(const Json::Value &payload) {
  subscriptions_.process(SubscriptionType::DAG_BLOCKS, payload);
}

void WsSession::newDagBlockFinalized(const Json::Value &payload) {
  subscriptions_.process(SubscriptionType::DAG_BLOCK_FINALIZED, payload);
}

void WsSession::newPbftBlockExecuted(const Json::Value &payload) {
  subscriptions_.process(SubscriptionType::PBFT_BLOCK_EXECUTED, payload);
}

void WsSession::newPillarBlockData(const Json::Value &payload) {
  subscriptions_.process(SubscriptionType::PILLAR_BLOCK, payload);
}

void WsSession::newPendingTransaction(const Json::Value &payload) {
  subscriptions_.process(SubscriptionType::TRANSACTIONS, payload);
}

void WsSession::newLogs(const final_chain::BlockHeader &header, TransactionHashes trx_hashes,
                        const TransactionReceipts &receipts) {
  subscriptions_.processLogs(header, trx_hashes, receipts);
}

void WsSession::close(bool normal) {
  closed_ = true;
  if (ws_.is_open()) {
    ws_.close(normal ? beast::websocket::normal : beast::websocket::abnormal);
  }
}

bool WsSession::is_normal(const beast::error_code &ec) const {
  if (ec == websocket::error::closed || ec == boost::asio::error::eof) {
    return true;
  }
  return false;
}

WsServer::WsServer(std::shared_ptr<util::ThreadPool> thread_pool, tcp::endpoint endpoint, addr_t node_addr,
                   uint32_t max_pending_tasks)
    : ioc_(thread_pool->unsafe_get_io_context()),
      acceptor_(thread_pool->unsafe_get_io_context()),
      thread_pool_(thread_pool),
      kMaxPendingTasks(max_pending_tasks),
      node_addr_(std::move(node_addr)) {
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
  for (auto const &session : sessions_) {
    if (!session->is_closed()) session->close();
  }
  sessions_.clear();
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
    auto session = sessions_.begin();
    while (session != sessions_.end()) {
      if ((*session)->is_closed()) {
        sessions_.erase(session++);
      } else {
        session++;
      }
    }
    // Create the session and run it
    sessions_.push_back(createSession(std::move(socket)));
    sessions_.back()->run();
  }

  // Accept another connection
  if (!stopped_) do_accept();
}

void WsServer::newEthBlock(const ::taraxa::final_chain::BlockHeader &header, const TransactionHashes &trx_hashes) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  if (sessions_.empty()) return;

  auto payload = rpc::eth::toJson(header);
  payload["transactions"] = rpc::eth::toJsonArray(trx_hashes);

  for (auto const &session : sessions_) {
    if (!session->is_closed()) {
      session->newEthBlock(payload);
    }
  }
}

void WsServer::newLogs(const ::taraxa::final_chain::BlockHeader &header, TransactionHashes trx_hashes,
                       const TransactionReceipts &receipts) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  if (sessions_.empty()) return;

  for (auto const &session : sessions_) {
    if (!session->is_closed()) {
      session->newLogs(header, trx_hashes, receipts);
    }
  }
}

void WsServer::newDagBlock(const std::shared_ptr<DagBlock> &blk) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  if (sessions_.empty()) return;

  auto payload = blk->getJson();
  for (auto const &session : sessions_) {
    if (!session->is_closed()) session->newDagBlock(payload);
  }
}

void WsServer::newDagBlockFinalized(const blk_hash_t &hash, uint64_t period) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  if (sessions_.empty()) return;

  Json::Value payload;
  payload["block"] = dev::toJS(hash);
  payload["period"] = dev::toJS(period);

  for (auto const &session : sessions_) {
    if (!session->is_closed()) session->newDagBlockFinalized(payload);
  }
}

void WsServer::newPbftBlockExecuted(const PbftBlock &pbft_blk,
                                    const std::vector<blk_hash_t> &finalized_dag_blk_hashes) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  if (sessions_.empty()) return;

  auto payload = PbftBlock::toJson(pbft_blk, finalized_dag_blk_hashes);

  for (auto const &session : sessions_) {
    if (!session->is_closed()) session->newPbftBlockExecuted(payload);
  }
}
void WsServer::newPendingTransaction(const trx_hash_t &trx_hash) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  if (sessions_.empty()) return;

  auto payload = dev::toJS(trx_hash);

  for (auto const &session : sessions_) {
    if (!session->is_closed()) session->newPendingTransaction(payload);
  }
}

void WsServer::newPillarBlockData(const pillar_chain::PillarBlockData &pillar_block_data) {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  if (sessions_.empty()) return;

  auto payload = pillar_block_data.getJson(true);

  for (auto const &session : sessions_) {
    if (!session->is_closed()) session->newPillarBlockData(payload);
  }
}

uint32_t WsServer::numberOfSessions() {
  boost::shared_lock<boost::shared_mutex> lock(sessions_mtx_);
  return sessions_.size();
}

uint32_t WsServer::numberOfPendingTasks() const {
  auto thread_pool = thread_pool_.lock();
  if (thread_pool) {
    return thread_pool->num_pending_tasks();
  }
  return 0;
}

}  // namespace taraxa::net
