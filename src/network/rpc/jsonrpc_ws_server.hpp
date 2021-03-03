#pragma once

#include "network/ws_server.hpp"

namespace taraxa::net {

class JsonRpcWSSession : public WSSession {
 public:
  using WSSession::WSSession;
  virtual std::string processRequest(const std::string& request) override;
};

class JsonRpcWsServer : public WSServer {
 public:
  using WSServer::WSServer;
  virtual std::shared_ptr<WSSession> createSession(tcp::socket& socket) override;
};

}  // namespace taraxa::net
