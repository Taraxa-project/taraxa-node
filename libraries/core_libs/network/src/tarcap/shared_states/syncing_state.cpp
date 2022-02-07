#include "network/tarcap/shared_states/syncing_state.hpp"

#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

SyncingState::SyncingState(uint16_t deep_syncing_threshold) : kDeepSyncingThreshold(deep_syncing_threshold) {}

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
    deep_pbft_syncing_ = (peer_->pbft_chain_size_ - period >= kDeepSyncingThreshold);
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
    deep_pbft_syncing_ = (peer_->pbft_chain_size_ - current_period >= kDeepSyncingThreshold);
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

bool SyncingState::is_syncing() { return is_pbft_syncing(); }

bool SyncingState::is_deep_pbft_syncing() const { return deep_pbft_syncing_; }

bool SyncingState::is_pbft_syncing() {
  if (!is_actively_syncing()) {
    set_pbft_syncing(false);
  }
  return pbft_syncing_;
}

}  // namespace taraxa::network::tarcap