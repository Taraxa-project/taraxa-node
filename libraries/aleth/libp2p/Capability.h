// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#pragma once

#include "Common.h"

namespace dev {
namespace p2p {

/**
 * @brief The Capability interface.
 * This interface has to be implemented to support any devp2p subprotocol.
 */

class CapabilityFace {
 public:
  virtual ~CapabilityFace() = default;

  /// Subprotocol name, used in negotiating common capabilities with the peers.
  virtual std::string name() const = 0;
  /// Subprotocol version, used in negotiating common capabilities with the
  /// peers.
  virtual unsigned version() const = 0;
  /// Number of messages supported by the subprotocol version.
  virtual unsigned messageCount() const = 0;
  /// Convert supplied packet type to string - used for logging purposes
  virtual std::string packetTypeToString(unsigned _packetType) const = 0;
  /// Called by the Host when new peer is connected.
  /// Guaranteed to be called first before any interpretCapabilityPacket for
  /// this peer.
  virtual void onConnect(std::weak_ptr<Session> session, u256 const& _peerCapabilityVersion) = 0;
  /// Called by the Host when the messaege is received from the peer
  /// @returns true if the message was interpreted, false if the message had not
  /// supported type.
  virtual void interpretCapabilityPacket(std::weak_ptr<Session> session, unsigned packet_type, RLP const& payload) = 0;
  /// Called by the Host when the peer is disconnected.
  /// Guaranteed to be called last after any interpretCapabilityPacket for this
  /// peer.
  virtual void onDisconnect(NodeID const& _nodeID) = 0;
};

}  // namespace p2p

}  // namespace dev
