#include "syncing_state.hpp"

namespace taraxa {

void SyncingState::set_peer(const dev::p2p::NodeID& peer_id) {
  std::lock_guard<std::shared_mutex> lock(peer_mutex_);
  peer_id_ = peer_id;
}

dev::p2p::NodeID SyncingState::syncing_peer() const {
  std::shared_lock<std::shared_mutex> lock(peer_mutex_);
  return peer_id_;
}

void SyncingState::set_pbft_syncing(bool syncing, const std::optional<dev::p2p::NodeID>& peer_id) {
  pbft_syncing_ = syncing;

  if (peer_id) {
    set_peer(peer_id.value());
  }

  // Reset last sync packet time when syncing is restarted/fresh syncing flag is set
  set_last_sync_packet_time();
}

void SyncingState::set_dag_syncing(bool syncing, const std::optional<dev::p2p::NodeID>& peer_id) {
  dag_syncing_ = syncing;

  if (peer_id) {
    set_peer(peer_id.value());
  }
}

void SyncingState::set_last_sync_packet_time() {
  std::lock_guard<std::shared_mutex> lock(time_mutex_);
  last_received_sync_packet_time_ = std::chrono::steady_clock::now();
}

bool SyncingState::is_actively_syncing() const {
  auto now = std::chrono::steady_clock::now();

  std::shared_lock<std::shared_mutex> lock(time_mutex_);
  return std::chrono::duration_cast<std::chrono::seconds>(now - last_received_sync_packet_time_) <=
         SYNCING_INACTIVITY_THRESHOLD;
}

bool SyncingState::is_syncing() const { return is_pbft_syncing() || is_dag_syncing(); }

bool SyncingState::is_pbft_syncing() const { return pbft_syncing_.load(); }

bool SyncingState::is_dag_syncing() const { return dag_syncing_.load(); }

}  // namespace taraxa