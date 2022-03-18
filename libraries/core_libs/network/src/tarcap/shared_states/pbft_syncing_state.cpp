#include "network/tarcap/shared_states/pbft_syncing_state.hpp"

#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

PbftSyncingState::PbftSyncingState(uint16_t deep_syncing_threshold) : kDeepSyncingThreshold(deep_syncing_threshold) {}

const dev::p2p::NodeID PbftSyncingState::syncingPeer() const {
  std::shared_lock lock(peer_mutex_);
  if (peer_) {
    return peer_->getId();
  }

  return dev::p2p::NodeID();
}

void PbftSyncingState::setSyncStatePeriod(uint64_t period) {
  if (pbft_syncing_) {
    std::shared_lock lock(peer_mutex_);
    deep_pbft_syncing_ = (peer_->pbft_chain_size_ - period >= kDeepSyncingThreshold);
  } else {
    deep_pbft_syncing_ = false;
  }
}

void PbftSyncingState::setPbftSyncing(bool syncing, uint64_t current_period,
                                      std::shared_ptr<TaraxaPeer> peer /*=nullptr*/) {
  assert((syncing && peer) || !syncing);
  pbft_syncing_ = syncing;

  std::unique_lock lock(peer_mutex_);
  peer_ = std::move(peer);

  if (syncing) {
    deep_pbft_syncing_ = (peer_->pbft_chain_size_ - current_period >= kDeepSyncingThreshold);
    // Reset last sync packet time when syncing is restarted/fresh syncing flag is set
    setLastSyncPacketTime();
  } else {
    deep_pbft_syncing_ = false;
  }
}

void PbftSyncingState::setLastSyncPacketTime() {
  std::unique_lock lock(time_mutex_);
  last_received_sync_packet_time_ = std::chrono::steady_clock::now();
}

bool PbftSyncingState::isActivelySyncing() const {
  auto now = std::chrono::steady_clock::now();

  std::shared_lock lock(time_mutex_);
  return std::chrono::duration_cast<std::chrono::seconds>(now - last_received_sync_packet_time_) <=
         kSyncingInactivityThreshold;
}

bool PbftSyncingState::isDeepPbftSyncing() const { return deep_pbft_syncing_; }

bool PbftSyncingState::isPbftSyncing() {
  if (!isActivelySyncing()) {
    setPbftSyncing(false);
  }

  return pbft_syncing_;
}

}  // namespace taraxa::network::tarcap