#pragma once

#include "network/ws_server.hpp"

namespace taraxa::net {

class JsonRpcWsSession final : public WsSession {
 public:
  using WsSession::WsSession;
  std::string processRequest(const std::string_view& request) override;
};

class JsonRpcWsServer final : public WsServer {
 public:
  using WsServer::WsServer;
  std::shared_ptr<WsSession> createSession(tcp::socket&& socket) override;
};

}  // namespace taraxa::net