#pragma once

#include "common/util.hpp"
#include "config/config.hpp"
#include "libp2p/Common.h"
#include "libp2p/Host.h"
#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/stats/time_period_packets_stats.hpp"
#include "network/tarcap/taraxa_peer.hpp"

namespace taraxa {
class PbftManager;
}

namespace taraxa::network::tarcap {

/**
 * @brief PeersState contains common members and functions related to peers, host, etc... that are shared among multiple
 * classes
 */
class PeersState {
 public:
  using PeersMap = std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>>;

  PeersState(std::weak_ptr<dev::p2p::Host> host, const FullNodeConfig& conf);

  std::shared_ptr<TaraxaPeer> getPeer(const dev::p2p::NodeID& node_id) const;
  std::shared_ptr<TaraxaPeer> getPendingPeer(const dev::p2p::NodeID& node_id) const;

  /**
   * @brief Get known peer based on packet sender and packet type. For StatusPacket peer can be obtained from
   *        pending_peers, for all other packet types peer can be obtained only from peers map, in which are only
   *        peers that already sent initial StatusPacket
   *
   * @return <std::shared_ptr<TaraxaPeer>, ""> if packet sender is known peer, otherwise <nullptr, "err message">
   */
  std::pair<std::shared_ptr<TaraxaPeer>, std::string> getPacketSenderPeer(const dev::p2p::NodeID& node_id,
                                                                          SubprotocolPacketType packet_type) const;

  PeersMap getAllPeers() const;
  std::vector<dev::p2p::NodeID> getAllPendingPeersIDs() const;
  size_t getPeersCount() const;
  std::shared_ptr<TaraxaPeer> addPendingPeer(const dev::p2p::NodeID& node_id, const std::string& address);
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

  /**
   * @brief Handle malicious peer
   * @param id
   */
  void handleMaliciousSyncPeer(const dev::p2p::NodeID& id);

  /**
   * @param filter_func
   * @return TaraxaPeer shared_ptr with max chain size
   */
  std::shared_ptr<TaraxaPeer> getMaxChainPeer(
      const std::shared_ptr<PbftManager> pbft_mgr, std::function<bool(const std::shared_ptr<TaraxaPeer>&)> filter_func =
                                                       [](const std::shared_ptr<TaraxaPeer>&) { return true; });

 private:
  /**
   * @brief Disconnect peer
   * @param id
   */
  void disconnectPeer(const dev::p2p::NodeID& id);

 public:
  const std::weak_ptr<dev::p2p::Host> host_;

 private:
  mutable std::shared_mutex peers_mutex_;
  PeersMap peers_;
  PeersMap pending_peers_;

  ThreadSafeMap<dev::p2p::NodeID, std::chrono::steady_clock::time_point> malicious_peers_;
  const FullNodeConfig kConf;
};

}  // namespace taraxa::network::tarcap
