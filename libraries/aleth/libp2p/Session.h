// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/Guards.h>
#include <libdevcore/RLP.h>

#include <array>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <utility>

#include "Capability.h"
#include "Common.h"
#include "Peer.h"
#include "RLPXSocket.h"
#include "taraxa.hpp"

namespace dev {

namespace p2p {

class Peer;
class RLPXFrameCoder;

/**
 * @brief The Session class
 * @todo Document fully.
 */
struct Session final : std::enable_shared_from_this<Session> {
 private:
  Session(SessionCapabilities caps, std::unique_ptr<RLPXFrameCoder> _io, std::shared_ptr<RLPXSocket> _s,
          std::shared_ptr<Peer> _n, PeerSessionInfo _info,
          std::optional<DisconnectReason> immediate_disconnect_reason = {});

 public:
  static std::shared_ptr<Session> make(SessionCapabilities caps, std::unique_ptr<RLPXFrameCoder> _io,
                                       std::shared_ptr<RLPXSocket> _s, std::shared_ptr<Peer> _n, PeerSessionInfo _info,
                                       std::optional<DisconnectReason> immediate_disconnect_reason = {});
  ~Session();

  void disconnect(DisconnectReason _reason) {
    muted_ = true;
    ba::post(m_socket->ref().get_executor(), [=, this, _ = shared_from_this()] { disconnect_(_reason); });
  }

  void send(std::string capability_name, unsigned packet_type, bytes payload, std::function<void()>&& on_done = {}) {
    ba::post(m_socket->ref().get_executor(),
             [=, this, _ = shared_from_this(), capability_name = std::move(capability_name),
              payload = std::move(payload), on_done = std::move(on_done)]() mutable {
               auto cap_itr = m_capabilities.find(capability_name);
               assert(cap_itr != m_capabilities.end());
               auto header = packet_type + cap_itr->second.offset;
               assert(header <= std::numeric_limits<byte>::max());
               bytes msg(1 + payload.size());
               msg[0] = header;
               memmove(msg.data() + 1, payload.data(), payload.size());
               send_(std::move(msg), std::move(on_done));
             });
  }

  void ping() {
    ba::post(m_socket->ref().get_executor(), [this, _ = shared_from_this()] { ping_(); });
  }

  bool isConnected() const { return m_socket->ref().is_open(); }

  NodeID id() const { return m_peer->id; }

  PeerSessionInfo info() const {
    std::shared_lock l(x_info);
    return m_info;
  }

 private:
  void disconnect_(DisconnectReason _reason);

  void ping_();

  void sealAndSend_(RLPStream& _s);

  std::shared_ptr<Peer> peer() const { return m_peer; }

  static RLPStream& prep(RLPStream& _s, P2pPacketType _t, unsigned _args = 0);

  void send_(bytes _msg, std::function<void()> on_done = {});

  /// Drop the connection for the reason @a _r.
  void drop(DisconnectReason _r);

  /// Perform a read on the socket.
  void doRead();

  /// Check error code after reading and drop peer if error code.
  bool checkRead(std::size_t expected, boost::system::error_code ec, std::size_t length);

  /// Perform a single round of the write operation. This could end up calling
  /// itself asynchronously.
  void write(uint16_t sequence_id = 0, uint32_t sent_size = 0);

  void splitAndPack(uint16_t& sequence_id, uint32_t& sent_size);

  /// Deliver RLPX packet to Session or PeerCapability for interpretation.
  void readPacket(unsigned _t, RLP const& _r);

  struct UnknownP2PPacketType : std::runtime_error {
    using runtime_error::runtime_error;
  };
  /// Interpret an incoming Session packet.
  void interpretP2pPacket(P2pPacketType _t, RLP const& _r);

  /// @returns true iff the _msg forms a valid message for sending or receiving
  /// on the network.
  static bool checkPacket(bytesConstRef _msg);

  std::optional<SessionCapability> capabilityFor(unsigned _packetType) const;

  static std::string capabilityPacketTypeToString(std::optional<SessionCapability> const& cap, unsigned _packetType);

  std::string capabilityPacketTypeToString(unsigned _packetType) const;

  std::atomic<bool> muted_ = false;
  /// The peer's capability set.
  SessionCapabilities m_capabilities;
  std::unique_ptr<RLPXFrameCoder> m_io;  ///< Transport over which packets are sent.
  std::shared_ptr<RLPXSocket> m_socket;  ///< Socket of peer's connection.
  struct SendRequest {
    bytes payload;
    std::function<void()> on_done;
  };
  std::deque<SendRequest> m_writeQueue;  ///< The write queue.
  std::vector<byte> m_data;              ///< Buffer for ingress packet data.
  std::vector<byte> m_multiData;         ///< Buffer for multipacket data.
  bytes m_out;

  std::shared_ptr<Peer> m_peer;  ///< The Peer object.
  bool m_dropped = false;        ///< If true, we've already divested ourselves of this peer. We're
                                 ///< just waiting for the reads & writes to fail before the
                                 ///< shared_ptr goes OOS and the destructor kicks in.

  mutable std::shared_mutex x_info;
  PeerSessionInfo m_info;  ///< Dynamic information about this peer.

  std::chrono::steady_clock::time_point m_ping;  ///< Time point of last ping.
  std::optional<DisconnectReason> immediate_disconnect_reason_;

  std::string m_logSuffix;

  Logger m_netLogger{createLogger(VerbosityDebug, "net")};
  Logger m_netLoggerDetail{createLogger(VerbosityTrace, "net")};
  Logger m_netLoggerError{createLogger(VerbosityError, "net")};

  Logger m_capLogger{createLogger(VerbosityDebug, "p2pcap")};
  Logger m_capLoggerDetail{createLogger(VerbosityTrace, "p2pcap")};
};

}  // namespace p2p
}  // namespace dev
