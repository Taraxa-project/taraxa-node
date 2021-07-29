#pragma once

#include <libp2p/Common.h>

#include <shared_mutex>

#include "logger/log.hpp"
#include "taraxa_peer.hpp"

namespace taraxa {

class PeersState {
 public:
  PeersState(addr_t const &node_addr) { LOG_OBJECTS_CREATE("PEERS"); }

  std::shared_ptr<TaraxaPeer> getPeer(dev::p2p::NodeID const &node_id) const;
  std::shared_ptr<TaraxaPeer> getPendingPeer(dev::p2p::NodeID const &node_id) const;
  size_t getPeersCount() const;
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> getAllPeers() const;
  std::vector<dev::p2p::NodeID> getAllPeersIDs() const;
  std::vector<dev::p2p::NodeID> getAllPendingPeers() const;

  void erasePeer(dev::p2p::NodeID const &node_id);
  std::shared_ptr<TaraxaPeer> addPendingPeer(dev::p2p::NodeID const &node_id);
  std::shared_ptr<TaraxaPeer> setPeerAsReadyToSendMessages(dev::p2p::NodeID const &node_id,
                                                           std::shared_ptr<TaraxaPeer> peer);

  std::vector<dev::p2p::NodeID> selectPeers(std::function<bool(TaraxaPeer const &)> const &_predicate) const;
  static std::pair<std::vector<dev::p2p::NodeID>, std::vector<dev::p2p::NodeID>> randomPartitionPeers(
      std::vector<dev::p2p::NodeID> const &_peers, std::size_t _number);

 private:
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> peers_;
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> pending_peers_;

  static std::mt19937_64 urng_;  // Mersenne Twister psuedo-random number generator

  mutable std::shared_mutex peers_mutex_;

  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa