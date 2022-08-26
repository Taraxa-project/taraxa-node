#pragma once

#include "network/rpc/WSServer.h"

namespace taraxa::net {

class GraphQlWSSession : public WSSession {
 public:
  using WSSession::WSSession;
  virtual std::string processRequest(const std::string& request) override;

  void triggerTestSubscribtion(unsigned int number);
};

class GraphQlWsServer : public WSServer {
 public:
  using WSServer::WSServer;
  virtual std::shared_ptr<WSSession> createSession(tcp::socket& socket) override;
};

}  // namespace taraxa::net