#include "network/tarcap/shared_states/syncing_state.hpp"

#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

SyncingState::SyncingState(const NetworkConfig &conf) : conf_(conf) {}

void SyncingState::set_peer(std::shared_ptr<TaraxaPeer> &&peer) {
  std::unique_lock lock(peer_mutex_);
  // we set peer to null if dag and pbft syncing are not enabled
  if (peer == nullptr && pbft_syncing_) {
    return;
  }
  peer_ = std::move(peer);
}

const dev::p2p::NodeID SyncingState::syncing_peer() const {
  std::shared_lock lock(peer_mutex_);
  if (peer_) {
    return peer_->getId();
  }
  return dev::p2p::NodeID();
}

void SyncingState::setSyncStatePeriod(uint64_t period) {
  if (pbft_syncing_) {
    std::shared_lock lock(peer_mutex_);
    deep_pbft_syncing_ = (peer_->pbft_chain_size_ - period >= conf_.deep_syncing_threshold);
  } else {
    deep_pbft_syncing_ = false;
  }
}

void SyncingState::set_pbft_syncing(bool syncing, uint64_t current_period,
                                    std::shared_ptr<TaraxaPeer> peer /*=nullptr*/) {
  assert((syncing && peer) || !syncing);
  pbft_syncing_ = syncing;
  peer_ = std::move(peer);

  if (syncing) {
    std::shared_lock lock(peer_mutex_);
    deep_pbft_syncing_ = (peer_->pbft_chain_size_ - current_period >= conf_.deep_syncing_threshold);
    // Reset last sync packet time when syncing is restarted/fresh syncing flag is set
    set_last_sync_packet_time();
  } else {
    deep_pbft_syncing_ = false;
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
    malicious_peers_[peer_id.value()] = std::chrono::steady_clock::now();
    return;
  }

  // this lock is for peer_id_ not the malicious_peers_
  std::shared_lock lock(peer_mutex_);
  malicious_peers_[peer_->getId()] = std::chrono::steady_clock::now();
}

bool SyncingState::is_peer_malicious(const dev::p2p::NodeID &peer_id) {
  if (conf_.disable_peer_blacklist) {
    return false;
  }

  // Peers are marked malicious for the time defined in conf_.network_peer_blacklist_timeout
  if (auto i = malicious_peers_.find(peer_id); i != malicious_peers_.end()) {
    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - i->second).count() <=
        conf_.network_peer_blacklist_timeout) {
      return true;
    } else {
      malicious_peers_.erase(i);
    }
  }
  return false;
}

bool SyncingState::is_syncing() { return is_pbft_syncing(); }

bool SyncingState::is_deep_pbft_syncing() const { return deep_pbft_syncing_; }

bool SyncingState::is_pbft_syncing() {
  if (!is_actively_syncing()) {
    set_pbft_syncing(false);
  }
  return pbft_syncing_;
}

}  // namespace taraxa::network::tarcap