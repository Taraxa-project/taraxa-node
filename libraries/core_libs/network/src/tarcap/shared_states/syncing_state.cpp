#include "network/tarcap/shared_states/syncing_state.hpp"

#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

SyncingState::SyncingState(std::shared_ptr<PeersState> peers_state)
    : peers_state_(std::move(peers_state)), malicious_peers_(300, 50) {}

void SyncingState::set_peer(const dev::p2p::NodeID &peer_id) {
  std::unique_lock lock(peer_mutex_);
  peer_id_ = peer_id;
}

const dev::p2p::NodeID &SyncingState::syncing_peer() const {
  std::shared_lock lock(peer_mutex_);
  return peer_id_;
}

void SyncingState::set_pbft_syncing(bool syncing, const std::optional<dev::p2p::NodeID> &peer_id) {
  assert((syncing && peer_id) || !syncing);
  pbft_syncing_ = syncing;

  if (peer_id) {
    set_peer(peer_id.value());
  }

  if (syncing) {
    // Reset last sync packet time when syncing is restarted/fresh syncing flag is set
    set_last_sync_packet_time();
  }
}

void SyncingState::set_dag_syncing(bool syncing, const std::optional<dev::p2p::NodeID> &peer_id) {
  assert((syncing && peer_id) || !syncing);
  dag_syncing_ = syncing;

  if (peer_id) {
    set_peer(peer_id.value());
  }
}

void SyncingState::set_last_sync_packet_time() {
  std::unique_lock lock(time_mutex_);
  last_received_sync_packet_time_ = std::chrono::steady_clock::now();
}

bool SyncingState::is_actively_syncing() const {
  auto now = std::chrono::steady_clock::now();

  std::shared_lock lock(time_mutex_);
  return std::chrono::duration_cast<std::chrono::seconds>(now - last_received_sync_packet_time_) <=
         SYNCING_INACTIVITY_THRESHOLD;
}

void SyncingState::set_peer_malicious(const std::optional<dev::p2p::NodeID> &peer_id) {
  if (peer_id.has_value()) {
    malicious_peers_.insert(peer_id.value());
    return;
  }

  // this lock is for peer_id_ not the malicious_peers_
  std::shared_lock lock(peer_mutex_);
  malicious_peers_.insert(peer_id_);
}

bool SyncingState::is_peer_malicious(const dev::p2p::NodeID &peer_id) const { return malicious_peers_.count(peer_id); }

bool SyncingState::is_syncing() const { return is_pbft_syncing() || is_dag_syncing(); }

bool SyncingState::is_pbft_syncing() const { return pbft_syncing_; }

bool SyncingState::is_dag_syncing() const { return dag_syncing_; }

}  // namespace taraxa::network::tarcap