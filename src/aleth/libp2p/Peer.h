// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2015-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include "Common.h"

namespace dev {

namespace p2p {

/**
 * @brief Representation of connectivity state and all other pertinent Peer
 * metadata. A Peer represents connectivity between two nodes, which in this
 * case, are the host and remote nodes.
 *
 * State information necessary for loading network topology is maintained by
 * NodeTable.
 *
 * @todo Implement 'bool required'
 * @todo reputation: Move score, rating to capability-specific map (&& remove
 * friend class)
 * @todo reputation: implement via origin-tagged events
 * @todo Populate metadata upon construction; save when destroyed.
 * @todo Metadata for peers needs to be handled via a storage backend.
 * Specifically, peers can be utilized in a variety of
 * many-to-many relationships while also needing to modify shared instances of
 * those peers. Modifying these properties via a storage backend alleviates
 * Host of the responsibility. (&& remove save/restoreNetwork)
 * @todo reimplement recording of historical session information on
 * per-transport basis
 * @todo move attributes into protected
 */
class Peer : public Node {
 public:
  using instant_t = std::chrono::system_clock::time_point;

  /// Construct Peer from Node.
  Peer(Node const& _node) : Node(_node), m_lastConnected(instant_t()), m_lastAttempted(instant_t()) {}

  Peer(Peer const&);

  /// Return true if connection attempt should be made to this peer or false if
  bool shouldReconnect() const;

  /// A peer which should never be reconnected to - e.g. it's running on a
  /// different network, we don't have any common capabilities
  bool isUseless() const;

  /// Returns number of seconds to wait until attempting connection, based on
  /// attempted connection history
  unsigned fallbackSeconds() const;

  /// Network Availability

  std::atomic<instant_t> m_lastConnected;
  std::atomic<instant_t> m_lastAttempted;
  std::atomic<unsigned> m_failedAttempts{0};
  std::atomic<DisconnectReason> m_lastDisconnect = NoDisconnect;  ///< Reason for disconnect that happened last.
  std::atomic<HandshakeFailureReason> m_lastHandshakeFailure =
      HandshakeFailureReason::NoFailure;  ///< Reason for most recent handshake
                                          ///< failure
};

}  // namespace p2p
}  // namespace dev
