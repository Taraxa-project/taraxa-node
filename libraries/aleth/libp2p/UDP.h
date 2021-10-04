// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "Common.h"
namespace ba = boost::asio;
namespace bi = ba::ip;

namespace dev {
namespace p2p {

/**
 * UDP Datagram
 * @todo make data protected/functional
 */
class UDPDatagram {
 public:
  explicit UDPDatagram(bi::udp::endpoint _ep) : locus(std::move(_ep)) {}
  UDPDatagram(bi::udp::endpoint _ep, bytes _data) : data(std::move(_data)), locus(std::move(_ep)) {}
  bi::udp::endpoint const& endpoint() const { return locus; }

  bytes data;

 protected:
  bi::udp::endpoint locus;
};

/**
 * @brief RLPX Datagram which can be signed.
 */
struct RLPXDatagramFace : public UDPDatagram {
  static uint32_t futureFromEpoch(std::chrono::seconds _sec) {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>((std::chrono::system_clock::now() + _sec).time_since_epoch())
            .count());
  }
  static uint32_t secondsSinceEpoch() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>((std::chrono::system_clock::now()).time_since_epoch())
            .count());
  }
  static Public authenticate(bytesConstRef _sig, bytesConstRef _rlp);

  explicit RLPXDatagramFace(bi::udp::endpoint const& _ep) : UDPDatagram(_ep) {}
  virtual ~RLPXDatagramFace() = default;

  virtual h256 sign(Secret const& _from);
  virtual uint8_t packetType() const = 0;

  virtual void streamRLP(RLPStream&) const = 0;
  virtual void interpretRLP(bytesConstRef _bytes) = 0;

  virtual std::string typeName() const = 0;
};

/**
 * @brief Interface which UDPSocket will implement.
 */
struct UDPSocketFace {
  virtual bool send(UDPDatagram const& _msg) = 0;
  virtual void disconnect() = 0;
};

/**
 * @brief Interface which a UDPSocket's owner must implement.
 */
struct UDPSocketEvents {
  virtual ~UDPSocketEvents() = default;
  virtual void onSocketDisconnected(UDPSocketFace*) {}
  virtual void onPacketReceived(UDPSocketFace*, bi::udp::endpoint const& _from, bytesConstRef _packetData) = 0;
};

/**
 * @brief UDP Interface
 * Handler must implement UDPSocketEvents.
 *
 * @todo multiple endpoints (we cannot advertise 0.0.0.0)
 * @todo decouple deque from UDPDatagram and add ref() to datagram for
 * fire&forget
 */
template <typename Handler, unsigned MaxDatagramSize>
class UDPSocket : UDPSocketFace, public std::enable_shared_from_this<UDPSocket<Handler, MaxDatagramSize>> {
 public:
  enum { maxDatagramSize = MaxDatagramSize };
  static_assert((unsigned)maxDatagramSize < 65507u, "UDP datagrams cannot be larger than 65507 bytes");

  /// Create socket for specific endpoint.
  UDPSocket(ba::strand<ba::io_context::executor_type>& _strand, UDPSocketEvents& _host, bi::udp::endpoint _endpoint)
      : m_host(_host), m_endpoint(std::move(_endpoint)), m_socket(_strand.get_inner_executor()), strand_(_strand) {
    m_started.store(false);
    m_closed.store(true);
  };

  /// Create socket which listens to all ports.
  UDPSocket(ba::strand<ba::io_context::executor_type>& _strand, UDPSocketEvents& _host, unsigned _port)
      : m_host(_host), m_endpoint(bi::udp::v4(), _port), m_socket(_strand.get_inner_executor()), strand_(_strand) {
    m_started.store(false);
    m_closed.store(true);
  };
  virtual ~UDPSocket() { UDPSocket::disconnect(); }

  /// Socket will begin listening for and delivering packets
  void connect();

  /// Send datagram.
  bool send(UDPDatagram const& _datagram) override;

  /// Returns if socket is open.
  bool isOpen() { return !m_closed; }

  /// Disconnect socket.
  void disconnect() override { disconnectWithError(boost::asio::error::connection_reset); }

 protected:
  void doRead();

  void doWrite();

  void disconnectWithError(boost::system::error_code _ec);

  std::atomic<bool> m_started{};  ///< Atomically ensure connection is started once. Start
                                  ///< cannot occur unless m_started is false. Managed by
                                  ///< start and disconnectWithError.
  std::atomic<bool> m_closed{};   ///< Connection availability.

  UDPSocketEvents& m_host;       ///< Interface which owns this socket.
  bi::udp::endpoint m_endpoint;  ///< Endpoint which we listen to.

  Mutex x_sendQ;
  std::deque<UDPDatagram> m_sendQ;               ///< Queue for egress data.
  std::array<byte, maxDatagramSize> m_recvData;  ///< Buffer for ingress data.
  bi::udp::endpoint m_recvEndpoint;              ///< Endpoint data was received from.
  bi::udp::socket m_socket;                      ///< Boost asio udp socket.

  Mutex x_socketError;                      ///< Mutex for error which can be set from host or IO
                                            ///< thread.
  boost::system::error_code m_socketError;  ///< Set when shut down due to error.
  ba::strand<ba::io_context::executor_type>& strand_;
};

template <typename Handler, unsigned MaxDatagramSize>
void UDPSocket<Handler, MaxDatagramSize>::connect() {
  bool expect = false;
  if (!m_started.compare_exchange_strong(expect, true)) return;

  m_socket.open(bi::udp::v4());
  try {
    m_socket.bind(m_endpoint);
  } catch (...) {
    m_socket.bind(bi::udp::endpoint(bi::udp::v4(), m_endpoint.port()));
  }

  // clear write queue so reconnect doesn't send stale messages
  Guard l(x_sendQ);
  m_sendQ.clear();

  m_closed = false;
  doRead();
}

template <typename Handler, unsigned MaxDatagramSize>
bool UDPSocket<Handler, MaxDatagramSize>::send(UDPDatagram const& _datagram) {
  if (m_closed) return false;

  Guard l(x_sendQ);
  m_sendQ.push_back(_datagram);
  if (m_sendQ.size() == 1) doWrite();

  return true;
}

template <typename Handler, unsigned MaxDatagramSize>
void UDPSocket<Handler, MaxDatagramSize>::doRead() {
  if (m_closed) return;

  auto self(UDPSocket<Handler, MaxDatagramSize>::shared_from_this());
  m_socket.async_receive_from(
      boost::asio::buffer(m_recvData), m_recvEndpoint,
      boost::asio::bind_executor(strand_, [this, self](boost::system::error_code _ec, size_t _len) {
        if (m_closed) return disconnectWithError(_ec);

        if (_ec != boost::system::errc::success)
          cnetlog << "Receiving UDP message failed. " << _ec.value() << " : " << _ec.message();

        if (_len) m_host.onPacketReceived(this, m_recvEndpoint, bytesConstRef(m_recvData.data(), _len));
        doRead();
      }));
}

template <typename Handler, unsigned MaxDatagramSize>
void UDPSocket<Handler, MaxDatagramSize>::doWrite() {
  if (m_closed) return;

  const UDPDatagram& datagram = m_sendQ[0];
  auto self(UDPSocket<Handler, MaxDatagramSize>::shared_from_this());
  bi::udp::endpoint endpoint(datagram.endpoint());
  m_socket.async_send_to(boost::asio::buffer(datagram.data), endpoint,
                         [this, self, endpoint](boost::system::error_code _ec, std::size_t) {
                           if (m_closed) return disconnectWithError(_ec);

                           if (_ec != boost::system::errc::success)
                             cnetlog << "Failed delivering UDP message. " << _ec.value() << " : " << _ec.message();

                           Guard l(x_sendQ);
                           m_sendQ.pop_front();
                           if (m_sendQ.empty()) return;
                           doWrite();
                         });
}

template <typename Handler, unsigned MaxDatagramSize>
void UDPSocket<Handler, MaxDatagramSize>::disconnectWithError(boost::system::error_code _ec) {
  // If !started and already stopped, shutdown has already occured. (EOF or
  // Operation canceled)
  if (!m_started && m_closed && !m_socket.is_open() /* todo: veirfy this logic*/) return;

  assert(_ec);
  {
    // disconnect-operation following prior non-zero errors are ignored
    Guard l(x_socketError);
    if (m_socketError != boost::system::error_code()) return;
    m_socketError = _ec;
  }
  // TODO: (if non-zero error) schedule high-priority writes

  // prevent concurrent disconnect
  bool expected = true;
  if (!m_started.compare_exchange_strong(expected, false)) return;

  // set m_closed to true to prevent undeliverable egress messages
  bool wasClosed = m_closed;
  m_closed = true;

  // close sockets
  boost::system::error_code ec;
  m_socket.shutdown(bi::udp::socket::shutdown_both, ec);
  m_socket.close();

  // socket never started if it never left stopped-state (pre-handshake)
  if (wasClosed) return;

  m_host.onSocketDisconnected(this);
}

}  // namespace p2p
}  // namespace dev
