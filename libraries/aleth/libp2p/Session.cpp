// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "Session.h"

#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Exceptions.h>

#include <chrono>

#include "Host.h"
#include "RLPXFrameCoder.h"

using namespace std;
using namespace dev;
using namespace dev::p2p;

static constexpr uint32_t MIN_COMPRESSION_SIZE = 500;

Session::Session(SessionCapabilities caps, unique_ptr<RLPXFrameCoder> _io, std::shared_ptr<RLPXSocket> _s,
                 std::shared_ptr<Peer> _n, PeerSessionInfo _info,
                 std::optional<DisconnectReason> immediate_disconnect_reason)
    : m_capabilities(std::move(caps)),
      m_io(std::move(_io)),
      m_socket(std::move(_s)),
      m_peer(std::move(_n)),
      m_info(std::move(_info)),
      m_ping(chrono::steady_clock::time_point::max()),
      immediate_disconnect_reason_(immediate_disconnect_reason) {
  stringstream remoteInfoStream;
  remoteInfoStream << "(" << m_info.id << "@" << m_socket->remoteEndpoint() << ")";
  m_logSuffix = remoteInfoStream.str();
  auto const attr = boost::log::attributes::constant<std::string>{remoteInfoStream.str()};
  m_netLogger.add_attribute("Suffix", attr);
  m_netLoggerDetail.add_attribute("Suffix", attr);
  m_netLoggerError.add_attribute("Suffix", attr);
  m_capLogger.add_attribute("Suffix", attr);
  m_capLoggerDetail.add_attribute("Suffix", attr);

  m_peer->m_lastDisconnect = NoDisconnect;
}

std::shared_ptr<Session> Session::make(SessionCapabilities caps, std::unique_ptr<RLPXFrameCoder> _io,
                                       std::shared_ptr<RLPXSocket> _s, std::shared_ptr<Peer> _n, PeerSessionInfo _info,
                                       std::optional<DisconnectReason> immediate_disconnect_reason) {
  shared_ptr<Session> ret(new Session(std::move(caps), std::move(_io), std::move(_s), std::move(_n), std::move(_info),
                                      immediate_disconnect_reason));
  if (immediate_disconnect_reason) {
    ret->disconnect_(*immediate_disconnect_reason);
    return ret;
  }
  assert(!ret->m_capabilities.empty());
  ba::post(ret->m_socket->ref().get_executor(), [=] {
    for (auto const& [_, cap] : ret->m_capabilities) {
      cap.ref->onConnect(ret, cap.version);
    }
    ret->ping_();
    ret->doRead();
  });
  return ret;
}

Session::~Session() {
  cnetlog << "Closing peer session with " << m_logSuffix;
  m_peer->m_lastConnected = m_peer->m_lastAttempted.load() - chrono::seconds(1);
  drop(ClientQuit);
}

void Session::readPacket(unsigned _packetType, RLP const& _r) {
  if (muted_) {
    return;
  }
  auto cap = capabilityFor(_packetType);
  auto packet_type_str = capabilityPacketTypeToString(cap, _packetType);
  LOG(m_netLoggerDetail) << "Received " << packet_type_str << " (" << _packetType << ") from";
  if (_packetType < UserPacket) {
    string err_msg;
    try {
      interpretP2pPacket(static_cast<P2pPacketType>(_packetType), _r);
    } catch (RLPException const& e) {
      err_msg = e.what();
    } catch (UnknownP2PPacketType const& e) {
      err_msg = e.what();
    }
    if (!err_msg.empty()) {
      LOG(m_netLogger) << "Exception caught in p2p::Session::interpretP2pPacket(): " << err_msg
                       << ". PacketType: " << packet_type_str << " (" << _packetType << "). RLP: " << _r;
      disconnect_(BadProtocol);
    }
    return;
  }
  if (!cap) {
    LOG(m_netLogger) << "Disconnecting cause no capability for packet type: " << _packetType;
    disconnect_(BadProtocol);
    return;
  }
  cap->ref->interpretCapabilityPacket(weak_from_this(), _packetType - cap->offset, _r);
}

void Session::interpretP2pPacket(P2pPacketType _t, RLP const& _r) {
  LOG(m_capLoggerDetail) << p2pPacketTypeToString(_t) << " from";
  switch (_t) {
    case DisconnectPacket: {
      string reason = "Unspecified";
      auto r = (DisconnectReason)_r[0].toInt<int>();
      if (!_r[0].isInt())
        drop(BadProtocol);
      else {
        reason = reasonOf(r);
        LOG(m_capLogger) << "Disconnect (reason: " << reason << ") from";
        drop(DisconnectRequested);
      }
      break;
    }
    case PingPacket: {
      LOG(m_capLoggerDetail) << "Pong to";
      RLPStream s;
      sealAndSend_(prep(s, PongPacket));
      break;
    }
    case PongPacket: {
      std::unique_lock l(x_info);
      m_info.lastPing = std::chrono::steady_clock::now() - m_ping;
      LOG(m_capLoggerDetail) << "Ping latency: " << chrono::duration_cast<chrono::milliseconds>(m_info.lastPing).count()
                             << " ms";
      break;
    }
    default:
      stringstream ss;
      ss << "Unknown p2p packet type: " << _t;
      throw UnknownP2PPacketType(ss.str());
  }
}

void Session::ping_() {
  clog(VerbosityTrace, "p2pcap") << "Ping to " << m_logSuffix;
  RLPStream s;
  sealAndSend_(prep(s, PingPacket));
  m_ping = std::chrono::steady_clock::now();
}

RLPStream& Session::prep(RLPStream& _s, P2pPacketType _id, unsigned _args) {
  return _s.append((unsigned)_id).appendList(_args);
}

void Session::sealAndSend_(RLPStream& _s) {
  bytes b;
  _s.swapOut(b);
  send_(std::move(b));
}

bool Session::checkPacket(bytesConstRef _msg) {
  if (_msg[0] > 0x7f || _msg.size() < 2) {
    return false;
  }
  if (RLP(_msg.cropped(1), RLP::LaissezFaire).actualSize() + 1 != _msg.size()) {
    return false;
  }
  return true;
}

void Session::send_(bytes _msg, std::function<void()> on_done) {
  LOG(m_netLoggerDetail) << capabilityPacketTypeToString(_msg[0]) << " to";
  if (!checkPacket(&_msg)) {
    clog(VerbosityError, "net") << "Invalid packet constructed. Size: " << _msg.size()
                                << " bytes, message: " << toHex(_msg);
  }
  if (!isConnected()) {
    return;
  }
  m_writeQueue.emplace_back(SendRequest{std::move(_msg), std::move(on_done)});
  if (m_writeQueue.size() == 1) {
    write();
  }
}

void Session::splitAndPack(uint16_t& sequence_id, uint32_t& sent_size) {
  if (sequence_id) [[unlikely]] {
    // Sending last chunk
    if (m_writeQueue[0].payload.size() < sent_size + RLPXFrameCoder::MAX_PACKET_SIZE) {
      bytesConstRef data(m_writeQueue[0].payload.data() + sent_size, m_writeQueue[0].payload.size() - sent_size);
      if (data.size() < MIN_COMPRESSION_SIZE) {
        m_io->writeFrame(sequence_id, data, m_out);
      } else {
        m_io->writeCompressedFrame(sequence_id, data, m_out);
      }
      sequence_id = 0;  // means we are finished
    } else {
      m_io->writeCompressedFrame(
          sequence_id, bytesConstRef(m_writeQueue[0].payload.data() + sent_size, RLPXFrameCoder::MAX_PACKET_SIZE),
          m_out);
      sequence_id++;
      sent_size += RLPXFrameCoder::MAX_PACKET_SIZE;
    }
  } else [[likely]] {
    // Sending single chunk
    if (m_writeQueue[0].payload.size() < RLPXFrameCoder::MAX_PACKET_SIZE) [[likely]] {
      if (m_writeQueue[0].payload.size() < MIN_COMPRESSION_SIZE) [[likely]] {
        m_io->writeSingleFramePacket(&m_writeQueue[0].payload, m_out);
      } else [[unlikely]] {
        m_io->writeCompressedFrame(0, &m_writeQueue[0].payload, m_out);
      }
    } else [[unlikely]] {
      m_io->writeCompressedFrame(sequence_id, m_writeQueue[0].payload.size(),
                                 bytesConstRef(m_writeQueue[0].payload.data(), RLPXFrameCoder::MAX_PACKET_SIZE), m_out);
      sequence_id++;
      sent_size = RLPXFrameCoder::MAX_PACKET_SIZE;
    }
  }
}

void Session::write(uint16_t sequence_id, uint32_t sent_size) {
  splitAndPack(sequence_id, sent_size);
  ba::async_write(m_socket->ref(), ba::buffer(m_out),
                  [this, this_shared = shared_from_this(), sequence_id, sent_size](boost::system::error_code ec,
                                                                                   std::size_t /*length*/) {
                    // must check queue, as write callback can occur following
                    // dropped()
                    if (ec) [[unlikely]] {
                      LOG(m_netLogger) << "Error sending: " << ec.message();
                      drop(TCPError);
                      return;
                    }
                    if (!sequence_id) [[likely]] {
                      if (m_writeQueue[0].on_done != nullptr) {
                        m_writeQueue[0].on_done();
                      }
                      m_writeQueue.pop_front();
                      if (m_writeQueue.empty()) {
                        return;
                      }
                      write();
                    } else [[unlikely]] {
                      write(sequence_id, sent_size);
                    }
                  });
}

void Session::drop(DisconnectReason _reason) {
  muted_ = true;
  if (m_dropped) {
    return;
  }
  if (!immediate_disconnect_reason_) {
    for (auto& [_, cap] : m_capabilities) {
      cap.ref->onDisconnect(id());
    }
  }
  auto& socket = m_socket->ref();
  if (socket.is_open()) {
    try {
      boost::system::error_code ec;
      LOG(m_netLoggerDetail) << "Closing (" << reasonOf(_reason) << ") connection with";
      socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
      socket.close();
    } catch (...) {
    }
  }
  m_peer->m_lastDisconnect = _reason;
  m_dropped = true;
}

void Session::disconnect_(DisconnectReason _reason) {
  muted_ = true;
  if (isConnected()) {
    clog(VerbosityTrace, "p2pcap") << "Disconnecting (our reason: " << reasonOf(_reason) << ") from " << m_logSuffix;
    RLPStream s;
    prep(s, DisconnectPacket, 1) << (int)_reason;
    sealAndSend_(s);
  }
  drop(_reason);
}

void Session::doRead() {
  // ignore packets received while waiting to disconnect.
  if (m_dropped) {
    return;
  }

  auto self(shared_from_this());
  m_data.resize(h256::size);
  ba::async_read(
      m_socket->ref(), boost::asio::buffer(m_data, h256::size),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!checkRead(h256::size, ec, length)) {
          return;
        }
        if (!m_io->authAndDecryptHeader(bytesRef(m_data.data(), length))) {
          LOG(m_netLogger) << "Header decrypt failed";
          drop(BadProtocol);  // todo: better error
          return;
        }

        uint16_t hProtocolId;
        uint32_t hLength;
        uint8_t hPadding;
        bool hMultiFrame;
        uint16_t hSequenceId;
        uint32_t hTotalLength;
        try {
          RLPXFrameInfo header(bytesConstRef(m_data.data(), length));
          hProtocolId = header.protocolId;
          hLength = header.length;
          hPadding = header.padding;
          hMultiFrame = header.multiFrame;
          hSequenceId = header.sequenceId;
          hTotalLength = header.totalLength;
        } catch (std::exception const& _e) {
          LOG(m_netLogger) << "Exception decoding frame header RLP: " << _e.what() << " "
                           << bytesConstRef(m_data.data(), h128::size).cropped(3);
          drop(BadProtocol);
          return;
        }

        /// read padded frame and mac
        auto tlen = hLength + hPadding + h128::size;
        m_data.resize(tlen);
        if (hMultiFrame && hSequenceId == 0) [[unlikely]] {
          m_multiData.reserve(hTotalLength);
        }
        ba::async_read(
            m_socket->ref(), boost::asio::buffer(m_data, tlen),
            [this, self, hLength, hProtocolId, tlen, hMultiFrame](boost::system::error_code ec, std::size_t length) {
              if (!checkRead(tlen, ec, length)) {
                return;
              }
              if (!m_io->authAndDecryptFrame(bytesRef(m_data.data(), tlen))) {
                LOG(m_netLogger) << "Frame decrypt failed";
                drop(BadProtocol);  // todo: better error
                return;
              }
              auto packet_lenght = hLength;
              if (hProtocolId) {
                packet_lenght = m_io->decompressFrame(bytesRef(m_data.data(), hLength), m_data);
                if (packet_lenght <= 0) [[unlikely]] {
                  LOG(m_netLogger) << "Frame decompress failed";
                  drop(BadProtocol);
                  return;
                }
              }

              if (hMultiFrame) [[unlikely]] {
                m_multiData.insert(m_multiData.end(), std::make_move_iterator(m_data.begin()),
                                   std::make_move_iterator(m_data.begin() + packet_lenght));
                if (packet_lenght < RLPXFrameCoder::MAX_PACKET_SIZE) {
                  bytesConstRef frame(m_multiData.data(), m_multiData.size());
                  auto packetType =
                      static_cast<P2pPacketType>(RLP(frame.cropped(0, 1), RLP::LaissezFaire).toInt<unsigned>());
                  if (!checkPacket(frame)) {
                    auto packet_type_str = capabilityPacketTypeToString(capabilityFor(packetType), packetType);
                    LOG(m_netLogger) << "Received invalid packet. Packet type (possibly "
                                        "corrupted): "
                                     << packetType << " (" << packet_type_str << ")."
                                     << "Frame Size: " << frame.size() << ". Size encoded in RLP: "
                                     << RLP(frame.cropped(1), RLP::LaissezFaire).actualSize()
                                     << ". Message: " << toHex(frame) << endl;
                    disconnect_(BadProtocol);
                    return;
                  }
                  readPacket(packetType, RLP(frame.cropped(1)));
                  m_multiData.clear();
                }
              } else [[likely]] {
                bytesConstRef frame(m_data.data(), packet_lenght);
                auto packetType =
                    static_cast<P2pPacketType>(RLP(frame.cropped(0, 1), RLP::LaissezFaire).toInt<unsigned>());
                if (!checkPacket(frame)) {
                  auto packet_type_str = capabilityPacketTypeToString(capabilityFor(packetType), packetType);
                  LOG(m_netLogger) << "Received invalid packet. Packet type (possibly "
                                      "corrupted): "
                                   << packetType << " (" << packet_type_str << ")."
                                   << "Frame Size: " << frame.size()
                                   << ". Size encoded in RLP: " << RLP(frame.cropped(1), RLP::LaissezFaire).actualSize()
                                   << ". Message: " << toHex(frame) << endl;
                  disconnect_(BadProtocol);
                  return;
                }
                readPacket(packetType, RLP(frame.cropped(1)));
              }
              doRead();
            });
      });
}

bool Session::checkRead(std::size_t expected, boost::system::error_code ec, std::size_t length) {
  if (ec && ec.category() != boost::asio::error::get_misc_category() && ec.value() != boost::asio::error::eof) {
    LOG(m_netLogger) << "Error reading: " << ec.message();
    drop(TCPError);
    return false;
  }
  if (ec && length < expected) {
    LOG(m_netLogger) << "Error reading - Abrupt peer disconnect: " << ec.message();
    drop(TCPError);
    return false;
  }
  if (length != expected) {
    // with static m_data-sized buffer this shouldn't happen unless there's a
    // regression sec recommends checking anyways (instead of assert)
    LOG(m_netLoggerError) << "Error reading - TCP read buffer length differs "
                             "from expected frame size ("
                          << length << " != " << expected << ")";
    disconnect_(UserReason);
    return false;
  }
  return true;
}

std::optional<SessionCapability> Session::capabilityFor(unsigned _packetType) const {
  if (_packetType < UserPacket) {
    return {};
  }
  for (auto const& [k, v] : m_capabilities) {
    if (v.offset <= _packetType && _packetType < v.message_count + v.offset) {
      return v;
    }
  }
  return {};
}

std::string Session::capabilityPacketTypeToString(std::optional<SessionCapability> const& cap, unsigned _packetType) {
  if (_packetType < UserPacket) {
    return p2pPacketTypeToString(static_cast<P2pPacketType>(_packetType));
  }
  if (cap) {
    return cap->ref->packetTypeToString(_packetType - cap->offset);
  }
  return "Unknown";
}

std::string Session::capabilityPacketTypeToString(unsigned _packetType) const {
  return capabilityPacketTypeToString(capabilityFor(_packetType), _packetType);
}
