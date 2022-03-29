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

bool PbftSyncingState::updatePeerAskingPeriod(const dev::p2p::NodeID& peer_id, uint64_t period) {
  auto now = std::chrono::steady_clock::now();

  UpgradableLock lock(asking_period_mutex_);
  if (asking_period_peers_table_.contains(peer_id) && asking_period_peers_table_[peer_id].first >= period &&
      std::chrono::duration_cast<std::chrono::milliseconds>(now - asking_period_peers_table_[peer_id].second) <=
          kPeerAskingPeriodTimeout) {
    return false;
  }

  UpgradeLock locked(lock);
  asking_period_peers_table_[peer_id] = std::make_pair(period, now);

  return true;
}

bool PbftSyncingState::updatePeerSyncingPeriod(const dev::p2p::NodeID& peer_id, uint64_t period) {
  UpgradableLock lock(incoming_blocks_mutex_);
  if (incoming_blocks_period_peers_table_.contains(peer_id) && incoming_blocks_period_peers_table_[peer_id] >= period) {
    return false;
  }

  UpgradeLock locked(lock);
  incoming_blocks_period_peers_table_[peer_id] = period;

  return true;
}

}  // namespace taraxa::network::tarcap