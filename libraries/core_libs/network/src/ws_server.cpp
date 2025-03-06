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

  // Set a decorator to change the Server of the handshake
  ws_.set_option(websocket::stream_base::decorator([](websocket::response_type &res) {
    res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
  }));

  try {
    http::read(ws_.next_layer(), buffer, request_);

    LOG(log_si_) << "Received WebSocket Upgrade Request";
    for (const auto& header : request_) {
      LOG(log_si_) << header.name_string() << ": " << header.value();
    }
  }
  catch(...) {
    LOG(log_si_) << "Received WebSocket Upgrade Request: Exception";
    return close();
  }

  // Accept the websocket handshake
  ws_.async_accept(request_, beast::bind_front_handler(&WsSession::on_accept, shared_from_this()));
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

  LOG(log_tr_) << "WS READ " << (static_cast<char *>(read_buffer_.data().data()));

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

  LOG(log_tr_) << "Before executor.post ";
  boost::asio::post(executor, [this, request = std::move(request)]() mutable { 
    auto ws_server = ws_server_.lock();

    std::string ip = request_["X-Real-IP"];
    try {
      if(ip.empty()) {
        auto endpoint = ws_.next_layer().socket().remote_endpoint();
        ip = endpoint.address().to_string();
      }
      else {
        ip = std::string("X-Real-IP:") + ip;
      }
      if(ws_server->ip_blacklist_.contains(ip)) {
        const uint32_t blacklist_interval_seconds = 300;
        if(ws_server->ip_blacklist_[ip] + blacklist_interval_seconds < time(nullptr)) {
          ws_server->ip_blacklist_.erase(ip);
        } else { 
          LOG(log_er_) << "Connection closed - IP blacklisted " << ip;
          return close(true);
        }
      }
    }
    catch(...) {
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    writeAsync(processRequest(request)); 
    auto end_time = std::chrono::steady_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    {
      std::lock_guard<std::mutex> lock(ws_server->stats_->stats_mutex);
      auto extractMethod = [](const std::string& jsonString) -> std::vector<std::string> {
        std::vector<std::string> ret;
        size_t start = 0;
        std::string method_key = "\"method\":";
        size_t method_pos = jsonString.find(method_key, start);
        while(method_pos != std::string::npos) {        
          start = method_pos + method_key.length();
          while(start < jsonString.size() && (jsonString[start] == ' ' || jsonString[start] == '"')) {
            start++;
          }
          size_t end = jsonString.find('"', start);
          if (end != std::string::npos) {
            ret.push_back(jsonString.substr(start, end - start));
          }
          method_pos = jsonString.find(method_key, start);
        }
        return ret;
      };

      // Method stats
      auto methods = extractMethod(request);
      for(auto method : methods) {
        auto& method_stats = ws_server->stats_->requests_by_method[method];
        method_stats.count++;
        method_stats.total_time += (processing_time / methods.size());
      }

      try {
        // IP stats
        auto& ip_stats = ws_server->stats_->requests_by_ip[ip];  // Creates if not exists
        for(auto method : methods) {
          auto& method_stats = ip_stats[method];
          method_stats.count++;
          method_stats.total_time += (processing_time / methods.size());
        }
      }
      catch(...) {
        return;
      }


      // Overall stats
      ws_server->stats_->total_processing_time += processing_time;
      ws_server->stats_->total_requests++;
    }
  });
  LOG(log_tr_) << "After executor.post ";
}

void WsSession::writeAsync(std::string &&message) {
  if (closed_) return;

  LOG(log_tr_) << "WS WRITE " << message.c_str();
  auto executor = ws_.get_executor();
  if (!executor) {
    LOG(log_tr_) << "Executor missing - WS closed";
    closed_ = true;
    return;
  }

  LOG(log_tr_) << "Before executor.post ";
  boost::asio::post(write_strand_, [this, message = std::move(message)]() mutable { writeImpl(std::move(message)); });
  LOG(log_tr_) << "After executors.post ";
}

void WsSession::writeImpl(std::string &&message) {
  ws_.text(true);  // as we are using text msg here
  try {
    ws_.write(boost::asio::buffer(message));
  } catch (const boost::system::system_error &e) {
    LOG(log_nf_) << "WS closed in on_write " << e.what();
    return close(is_normal(e.code()));
  }
  LOG(log_tr_) << "WS WRITE COMPLETE " << &ws_;
}

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
    writeAsync(std::move(response));
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
    writeAsync(std::move(response));
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
    writeAsync(std::move(response));
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
    writeAsync(std::move(response));
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
    writeAsync(std::move(response));
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
    writeAsync(std::move(response));
  }
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

WsServer::WsServer(boost::asio::io_context &ioc, tcp::endpoint endpoint, addr_t node_addr, std::unordered_map<std::string, uint32_t> rpc_method_limits)
    : ioc_(ioc), acceptor_(ioc), node_addr_(std::move(node_addr)),
      stats_(std::make_shared<RequestStats>()),
    stats_timer_(ioc, std::chrono::minutes(1)), rpc_method_limits_(rpc_method_limits) {
  LOG_OBJECTS_CREATE("WS_SERVER");
  beast::error_code ec;
  
  startStatsLogging();

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

void WsServer::startStatsLogging() {
  stats_timer_.async_wait([this](boost::system::error_code const& ec) {
    if (!ec) {
      logStats();
      stats_timer_.expires_after(std::chrono::minutes(1));
      startStatsLogging();
    } else {
      LOG(log_er_) << "Stats timer error: " << ec.message();
    }
  });
}

void WsServer::logStats() {
  std::lock_guard<std::mutex> lock(stats_->stats_mutex);
  LOG(log_si_) << "Request Statistics (last minute):";
  LOG(log_si_) << "  By IP:";
  for (const auto& [ip, stats_ip] : stats_->requests_by_ip) {
    for (const auto& [method, stats] : stats_ip) {
      auto method_limit = rpc_method_limits_.find(method);
      if(method_limit != rpc_method_limits_.end()) {
        if(method_limit->second < stats.count) {
          LOG(log_si_) << "Blacklisting: " << ip;
          ip_blacklist_[ip] = time(nullptr);
        }
      }
      LOG(log_si_) << "    " << ip << ": " << method << ": " << stats.count << " requests, Avg Time: " << stats.averageTimeMs() << " ms";
    }
  }
  LOG(log_si_) << "  By Method:";
  for (const auto& [method, stats] : stats_->requests_by_method) {
    LOG(log_si_) << "    " << method << ": " << stats.count << " requests, Avg Time: " << stats.averageTimeMs() << " ms";
  }
  LOG(log_si_) << "  Overall Average Processing Time: " << stats_->averageProcessingTimeMs() << " ms";

  // Reset stats for the next minute
  stats_->requests_by_ip.clear();
  stats_->requests_by_method.clear();
  stats_->total_processing_time = std::chrono::microseconds{0};
  stats_->total_requests = 0;
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