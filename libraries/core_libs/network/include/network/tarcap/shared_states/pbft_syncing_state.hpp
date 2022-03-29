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
  void setPbftSyncing(bool syncing, uint64_t current_period = 0, std::shared_ptr<TaraxaPeer> peer = nullptr);

  /**
   * @brief Set current time as last received sync packet time
   */
  void setLastSyncPacketTime();

  /**
   * @brief Set current pbft period
   */
  void setSyncStatePeriod(uint64_t period);

  /**
   * @brief Check if syncing is active
   *
   * @return true if last syncing packet was received within kSyncingInactivityThreshold
   */
  bool isActivelySyncing() const;

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
   * @brief Get the peer ID that own is syncing with
   *
   * @return peer ID if has a peer that own is syncing with
   */
  const dev::p2p::NodeID syncingPeer() const;

  /**
   * @brief Update peers asking sync period table
   *
   * @param peer_id
   * @param period peer asking sync period
   *
   * @return true if table not include peer or asking sync period greater than saving value or has excessed the
   * kPeerAskingPeriodTimeout for peer restart/rebuild DB situation. Otherwise return false.
   */
  bool updatePeerAskingPeriod(const dev::p2p::NodeID& peer_id, uint64_t period);

  /**
   * @brief Update peers sending sync blocks period
   *
   * @param peer_id
   * @param period peer sending sync block period
   *
   * @return true if table not include peer or sending sync block period greater than saving value. Otherwise return
   * false
   */
  bool updatePeerSyncingPeriod(const dev::p2p::NodeID& peer_id, uint64_t period);

 private:
  using UpgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using UpgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  std::atomic<bool> deep_pbft_syncing_{false};
  std::atomic<bool> pbft_syncing_{false};

  const uint16_t kDeepSyncingThreshold;

  // Number of seconds needed for ongoing syncing to be declared as inactive
  static constexpr std::chrono::seconds kSyncingInactivityThreshold{60};

  // Number of milliseconds for peer asking sync period timeout
  // corner case restart/rebuild DB may ask previous periods again
  const std::chrono::milliseconds kPeerAskingPeriodTimeout{1000};

  // What time was received last syncing packet
  std::chrono::steady_clock::time_point last_received_sync_packet_time_{std::chrono::steady_clock::now()};
  mutable std::shared_mutex time_mutex_;

  // Peer that the node is syncing with
  std::shared_ptr<TaraxaPeer> peer_;
  mutable std::shared_mutex peer_mutex_;

  /**
   * @brief Table for peer asking sync period
   *        Table key: peer node ID, value: a pair
   *        Pair key: peer last asking request period, pair value: peer last asking request time point
   */
  std::unordered_map<dev::p2p::NodeID, std::pair<uint64_t, std::chrono::steady_clock::time_point>>
      asking_period_peers_table_;
  mutable boost::shared_mutex asking_period_mutex_;

  /**
   * @brief Table for incoming last PBFT blocks period for syncing per each peer.
   *        Key: peer node ID
   *        Value: last PBFT block period number
   */
  std::unordered_map<dev::p2p::NodeID, uint64_t> incoming_blocks_period_peers_table_;
  mutable boost::shared_mutex incoming_blocks_mutex_;
};

}  // namespace taraxa::network::tarcap
