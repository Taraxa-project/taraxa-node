#pragma once

#include <atomic>

#include "common/util.hpp"
#include "libp2p/Common.h"

namespace taraxa::network::tarcap {

class TaraxaPeer;

/**
 * @brief PbftSyncingState contains common members and functions related to syncing that are shared among multiple
 * classes
 */
class PbftSyncingState {
 public:
  PbftSyncingState(uint16_t deep_syncing_threshold);

  /**
   * @brief Set pbft syncing
   *
   * @param syncing
   * @param current_period
   * @param peer used in case syncing flag == true to set which peer is the node syncing with
   */
  bool setPbftSyncing(bool syncing, PbftPeriod current_period = 0, std::shared_ptr<TaraxaPeer> peer = nullptr);

  /**
   * @brief Set current pbft period
   */
  void setSyncStatePeriod(PbftPeriod period);

  /**
   * @brief Check if PBFT is in deep syncing
   *
   * @return true if PBFT is in deep syncing
   */
  bool isDeepPbftSyncing() const;

  /**
   * @brief Check if PBFT is in syncing. If not in active syncing, set PBFT syncing to false
   *
   * @return true if PBFT is in syncing
   */
  bool isPbftSyncing();

  /**
   * @brief Get the peer that our node is syncing with
   *
   * @return syncing peer, in case there is none - nullptr is returned
   */
  std::shared_ptr<TaraxaPeer> syncingPeer() const;

  /**
   * @brief Set current time as last received sync packet time
   */
  void setLastSyncPacketTime();

  /**
   * @brief Check if syncing is active
   *
   * @return true if last syncing packet was received within kSyncingInactivityThreshold
   */
  bool isActivelySyncing() const;

 private:
  std::atomic<bool> deep_pbft_syncing_{false};
  std::atomic<bool> pbft_syncing_{false};

  const uint16_t kDeepSyncingThreshold;

  // Number of seconds needed for ongoing syncing to be declared as inactive
  static constexpr std::chrono::seconds kSyncingInactivityThreshold{60};

  // What time was received last syncing packet
  std::chrono::steady_clock::time_point last_received_sync_packet_time_{std::chrono::steady_clock::now()};
  mutable std::shared_mutex time_mutex_;

  // Peer that the node is syncing with
  std::shared_ptr<TaraxaPeer> peer_;
  mutable std::shared_mutex peer_mutex_;
};

}  // namespace taraxa::network::tarcap
