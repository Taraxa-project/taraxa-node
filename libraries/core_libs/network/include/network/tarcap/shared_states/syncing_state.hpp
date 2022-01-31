#pragma once

#include <atomic>

#include "common/util.hpp"
#include "config/config.hpp"
#include "libp2p/Common.h"

namespace taraxa::network::tarcap {

class TaraxaPeer;

/**
 * @brief SyncingState contains common members and functions related to syncing that are shared among multiple classes
 */
class SyncingState {
 public:
  SyncingState(const NetworkConfig &conf);

  /**
   * @brief Set pbft syncing
   *
   * @param syncing
   * @param current_period
   * @param peer used in case syncing flag == true to set which peer is the node syncing with
   */
  void set_pbft_syncing(bool syncing, uint64_t current_period = 0, std::shared_ptr<TaraxaPeer> peer = nullptr);

  /**
   * @brief Set current time as last received sync packet  time
   */
  void set_last_sync_packet_time();

  /**
   * @brief Set current pbft period
   */
  void setSyncStatePeriod(uint64_t period);

  /**
   * @brief Check if syncing is active
   *
   * @return true if last syncing packet was received within SYNCING_INACTIVITY_THRESHOLD
   */
  bool is_actively_syncing() const;

  bool is_syncing();
  bool is_deep_pbft_syncing() const;
  bool is_pbft_syncing();

  const dev::p2p::NodeID syncing_peer() const;

  /**
   * @brief Marks peer as malicious, in case none is provided, peer_id_ (node that we currently syncing with) is marked
   * @param peer_id
   */
  void set_peer_malicious(const std::optional<dev::p2p::NodeID> &peer_id = {});
  bool is_peer_malicious(const dev::p2p::NodeID &peer_id);

 private:
  void set_peer(std::shared_ptr<TaraxaPeer> &&peer);

 private:
  std::atomic<bool> deep_pbft_syncing_{false};
  std::atomic<bool> pbft_syncing_{false};

  std::unordered_map<dev::p2p::NodeID, std::chrono::steady_clock::time_point> malicious_peers_;

  NetworkConfig conf_;

  // Number of seconds needed for ongoing syncing to be declared as inactive
  static constexpr std::chrono::seconds SYNCING_INACTIVITY_THRESHOLD{60};

  // What time was received last syncing packet
  std::chrono::steady_clock::time_point last_received_sync_packet_time_{std::chrono::steady_clock::now()};
  mutable std::shared_mutex time_mutex_;

  // Peer that the node is syncing with
  std::shared_ptr<TaraxaPeer> peer_;
  mutable std::shared_mutex peer_mutex_;
};

}  // namespace taraxa::network::tarcap
