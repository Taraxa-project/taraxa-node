#pragma once

#include <libp2p/Common.h>

#include <atomic>
#include <optional>
#include <shared_mutex>

namespace taraxa {

class SyncingState {
 public:
  /**
   * @brief Set pbft syncing
   *
   * @param syncing
   * @param peer_id used in case syncing flag == true to set which peer is the node syncing with
   */
  void set_pbft_syncing(bool syncing, const std::optional<dev::p2p::NodeID>& peer_id = {});

  /**
   * @brief Set dag syncing
   *
   * @param syncing
   * @param peer_id used in case syncing flag == true to set which peer is the node syncing with
   */
  void set_dag_syncing(bool syncing, const std::optional<dev::p2p::NodeID>& peer_id = {});

  /**
   * @brief Set current time as last received sync packet  time
   */
  void set_last_sync_packet_time();

  /**
   * @brief Check if syncing is active
   *
   * @return true if last syncing packet was received within SYNCING_INACTIVITY_THRESHOLD
   */
  bool is_actively_syncing() const;

  bool is_syncing() const;
  bool is_pbft_syncing() const;
  bool is_dag_syncing() const;

  dev::p2p::NodeID syncing_peer() const;

 private:
  void set_peer(const dev::p2p::NodeID& peer_id);

 private:
  std::atomic<bool> pbft_syncing_{false};
  std::atomic<bool> dag_syncing_{false};

  // Number of seconds needed for ongoing syncing to be declared as inactive
  std::chrono::seconds SYNCING_INACTIVITY_THRESHOLD{60};

  // What time was received last syncing packet
  std::chrono::steady_clock::time_point last_received_sync_packet_time_{std::chrono::steady_clock::now()};
  mutable std::shared_mutex time_mutex_;

  // Peer id that the node is syncing with
  dev::p2p::NodeID peer_id_;
  mutable std::shared_mutex peer_mutex_;
};

}  // namespace taraxa
