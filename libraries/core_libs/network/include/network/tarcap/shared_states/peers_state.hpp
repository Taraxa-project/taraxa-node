#pragma once

#include "common/util.hpp"
#include "config/config.hpp"
#include "dag/dag_block.hpp"
#include "libp2p/Common.h"
#include "libp2p/Host.h"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/stats/packets_stats.hpp"
#include "network/tarcap/taraxa_peer.hpp"
#include "transaction/transaction.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief PeersState contains common members and functions related to peers, host, etc... that are shared among multiple
 * classes
 */
class PeersState {
 public:
  PeersState(std::weak_ptr<dev::p2p::Host> host, const dev::p2p::NodeID& own_node_id, const NetworkConfig& conf);

  std::shared_ptr<TaraxaPeer> getPeer(const dev::p2p::NodeID& node_id) const;
  std::shared_ptr<TaraxaPeer> getPendingPeer(const dev::p2p::NodeID& node_id) const;
  /**
   * @brief Returns TaraxaPeer object for specified node_id and bool value that indicates if peer was still pending
   */
  std::pair<std::shared_ptr<TaraxaPeer>, bool> getAnyPeer(const dev::p2p::NodeID& node_id) const;
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> getAllPeers() const;
  std::vector<dev::p2p::NodeID> getAllPendingPeersIDs() const;
  size_t getPeersCount() const;
  std::shared_ptr<TaraxaPeer> addPendingPeer(const dev::p2p::NodeID& node_id);
  void erasePeer(const dev::p2p::NodeID& node_id);
  std::shared_ptr<TaraxaPeer> setPeerAsReadyToSendMessages(dev::p2p::NodeID const& node_id,
                                                           std::shared_ptr<TaraxaPeer> peer);

  /**
   * @brief Marks peer as malicious
   * @param peer_id
   */
  void set_peer_malicious(const dev::p2p::NodeID& peer_id);

  /**
   * @brief Checks if peer is in malicious peers list
   * @return returns true if peer is in malicious peer list
   */
  bool is_peer_malicious(const dev::p2p::NodeID& peer_id);

 public:
  const std::weak_ptr<dev::p2p::Host> host_;
  const dev::p2p::NodeID node_id_;

 private:
  mutable std::shared_mutex peers_mutex_;
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> peers_;
  std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> pending_peers_;

  ThreadSafeMap<dev::p2p::NodeID, std::chrono::steady_clock::time_point> malicious_peers_;
  const NetworkConfig conf_;
};

}  // namespace taraxa::network::tarcap
