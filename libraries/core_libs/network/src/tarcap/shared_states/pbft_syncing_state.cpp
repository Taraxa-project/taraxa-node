#include "network/tarcap/shared_states/pbft_syncing_state.hpp"

#include "network/tarcap/packet_types.hpp"
#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

PbftSyncingState::PbftSyncingState(uint16_t deep_syncing_threshold) : kDeepSyncingThreshold(deep_syncing_threshold) {}

std::shared_ptr<TaraxaPeer> PbftSyncingState::syncingPeer() const {
  std::shared_lock lock(peer_mutex_);
  return peer_;
}

void PbftSyncingState::setSyncStatePeriod(PbftPeriod period) {
  if (pbft_syncing_) {
    std::shared_lock lock(peer_mutex_);
    deep_pbft_syncing_ = (peer_->pbft_chain_size_ - period >= kDeepSyncingThreshold);
  } else {
    deep_pbft_syncing_ = false;
  }
}

bool PbftSyncingState::setPbftSyncing(bool syncing, PbftPeriod current_period,
                                      std::shared_ptr<TaraxaPeer> peer /*=nullptr*/) {
  assert((syncing && peer) || !syncing);

  // Flag was changed meanwhile we should not be updating it again
  if (pbft_syncing_ && syncing) {
    return false;
  }
  {
    std::unique_lock lock(peer_mutex_);
    pbft_syncing_ = syncing;
    peer_ = std::move(peer);

    if (syncing) {
      deep_pbft_syncing_ = (peer_->pbft_chain_size_ - current_period >= kDeepSyncingThreshold);
      // Reset last sync packet time when syncing is restarted/fresh syncing flag is set
      setLastSyncPacketTime();
    } else {
      deep_pbft_syncing_ = false;
    }
    return true;
  }
}

void PbftSyncingState::setLastSyncPacketTime() {
  std::unique_lock lock(time_mutex_);
  last_received_sync_packet_time_ = std::chrono::steady_clock::now();
}

bool PbftSyncingState::isActivelySyncing() const {
  std::shared_lock lock(time_mutex_);
  auto now = std::chrono::steady_clock::now();
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