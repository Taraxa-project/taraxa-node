#include "network/http_server.hpp"

namespace taraxa::net {

HttpServer::HttpServer(boost::asio::io_context &io, boost::asio::ip::tcp::endpoint ep, const addr_t &node_addr,
                       const std::shared_ptr<HttpProcessor> &request_processor, std::unordered_map<std::string, uint32_t> rpc_method_limits)
    : request_processor_(request_processor), stats_(std::make_shared<RequestStats>()),
    stats_timer_(io, std::chrono::minutes(1)), rpc_method_limits_(rpc_method_limits), io_context_(io), acceptor_(io), ep_(std::move(ep)) {

  LOG_OBJECTS_CREATE("HTTP");
  LOG(log_si_) << "Taraxa HttpServer started at port: " << ep_.port();
  startStatsLogging();
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

void HttpServer::startStatsLogging() {
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

void HttpServer::logStats() {
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
          assert(server_->request_processor_);
          LOG(server_->log_dg_) << "Received: " << request_;

          std::string ip = request_["X-Real-IP"];
          if(ip.empty()) {
            auto endpoint = socket_.remote_endpoint();
            ip = endpoint.address().to_string();
          }
          else {
            ip = std::string("X-Real-IP:") + ip;
          }
          if(server_->ip_blacklist_.contains(ip)) {
            const uint32_t blacklist_interval_seconds = 300;
            if(server_->ip_blacklist_[ip] + blacklist_interval_seconds < time(nullptr)) {
              server_->ip_blacklist_.erase(ip);
            } else { 
              LOG(server_->log_er_) << "Connection closed - IP blacklisted " << ip;
              stop();
              return;
            }
          }
          
          auto start_time = std::chrono::steady_clock::now();
          response_ = server_->request_processor_->process(request_);
          auto end_time = std::chrono::steady_clock::now();
          auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

          {
            std::lock_guard<std::mutex> lock(server_->stats_->stats_mutex);
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
            auto methods = extractMethod(request_.body());
            for(auto method : methods) {
              auto& method_stats = server_->stats_->requests_by_method[method];
              method_stats.count++;
              method_stats.total_time += (processing_time / methods.size());
            }

            try {
              // IP stats
              auto& ip_stats = server_->stats_->requests_by_ip[ip];  // Creates if not exists
              for(auto method : methods) {
                auto& method_stats = ip_stats[method];
                method_stats.count++;
                method_stats.total_time += (processing_time / methods.size());
              }
            }
            catch(...) {
              LOG(server_->log_dg_) << "remote_endpoint exception";
            }

            // Overall stats
            server_->stats_->total_processing_time += processing_time;
            server_->stats_->total_requests++;
          }

          boost::beast::http::async_write(
              socket_, response_,
              [this_sp = getShared()](auto const & /*ec*/, auto /*bytes_transferred*/) { this_sp->stop(); });
          
        }
      });
}

}  // namespace taraxa::net