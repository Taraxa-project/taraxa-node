#pragma once

#include "network/ws_server.hpp"

namespace taraxa::net {

class JsonRpcWSSession final : public WSSession {
 public:
  using WSSession::WSSession;
  std::string processRequest(const std::string_view& request) override;
};

class JsonRpcWsServer final : public WSServer {
 public:
  using WSServer::WSServer;
  std::shared_ptr<WSSession> createSession(tcp::socket&& socket) override;
};

}  // namespace taraxa::net