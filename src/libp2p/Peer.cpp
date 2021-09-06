// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "Peer.h"
using namespace std;
using namespace dev;
using namespace dev::p2p;

namespace dev {

namespace p2p {
namespace {
unsigned defaultFallbackSeconds(unsigned _failedAttempts) {
  if (_failedAttempts < 5)
    return _failedAttempts ? _failedAttempts * 5 : 5;
  else if (_failedAttempts < 15)
    return 25 + (_failedAttempts - 5) * 10;
  else
    return 25 + 100 + (_failedAttempts - 15) * 20;
}
}  // namespace

Peer::Peer(Peer const& _original)
    : Node(_original),
      m_lastConnected(_original.m_lastConnected.load()),
      m_lastAttempted(_original.m_lastAttempted.load()),
      m_lastDisconnect(_original.m_lastDisconnect.load()),
      m_lastHandshakeFailure(_original.m_lastHandshakeFailure.load()) {
  m_failedAttempts = _original.m_failedAttempts.load();
}

bool Peer::shouldReconnect() const {
  return id && get_endpoint() && !isUseless() &&
         chrono::system_clock::now() > m_lastAttempted.load() + chrono::seconds(fallbackSeconds());
}

bool Peer::isUseless() const {
  if (peerType == PeerType::Required) return false;

  switch (m_lastHandshakeFailure) {
    case HandshakeFailureReason::FrameDecryptionFailure:
    case HandshakeFailureReason::ProtocolError:
      return true;
    default:
      break;
  }

  switch (m_lastDisconnect) {
    // Critical cases
    case BadProtocol:
    case UselessPeer:
    case IncompatibleProtocol:
    case UnexpectedIdentity:
    case DuplicatePeer:
    case NullIdentity:
      return true;
    // Potentially transient cases which can resolve quickly
    case PingTimeout:
    case TCPError:
    case TooManyPeers:
      return m_failedAttempts >= 10;
    // Potentially transient cases which can take longer to resolve
    case ClientQuit:
    case UserReason:
      return m_failedAttempts >= 25;
    default:
      break;
  }
  return false;
}

unsigned Peer::fallbackSeconds() const {
  constexpr unsigned oneYearInSeconds{60 * 60 * 24 * 360};

  if (peerType == PeerType::Required) return 5;

  if (isUseless()) return oneYearInSeconds;

  switch (m_lastDisconnect) {
    case TCPError:
    case PingTimeout:
    case TooManyPeers:
      return 15 * (m_failedAttempts + 1);
    case ClientQuit:
    case UserReason:
      return 25 * (m_failedAttempts + 1);
    default:
      return defaultFallbackSeconds(m_failedAttempts);
  }
}

}  // namespace p2p
}  // namespace dev
