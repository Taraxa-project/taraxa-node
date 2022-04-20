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

void PbftSyncingState::updatePeerAskingPeriod(const dev::p2p::NodeID& peer_id, uint64_t period) {
  auto now = std::chrono::steady_clock::now();
  const auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - asking_period_peers_table_[peer_id].second);

  std::unique_lock lock(asking_period_mutex_);
  if (asking_period_peers_table_.contains(peer_id) && asking_period_peers_table_[peer_id].first >= period &&
      duration < kPeerAskingPeriodTimeout) {
    std::stringstream err;
    err << "Peer " << peer_id << " request syncing period " << period << " that is previous period. "
        << " The peer last request period is " << asking_period_peers_table_[peer_id].first << " that within "
        << duration.count() << " ms";
    asking_period_peers_table_.erase(peer_id);
    throw std::logic_error(err.str());
  }

  asking_period_peers_table_[peer_id] = std::make_pair(period, now);
}

void PbftSyncingState::updatePeerSyncingPeriod(const dev::p2p::NodeID& peer_id, uint64_t period) {
  std::unique_lock lock(incoming_blocks_mutex_);
  if (incoming_blocks_period_peers_table_.contains(peer_id) && incoming_blocks_period_peers_table_[peer_id] >= period) {
    std::stringstream err;
    err << "Peer " << peer_id << " sending previous period " << period
        << " PBFT block. The peer last sending PBFT block period is " << incoming_blocks_period_peers_table_[peer_id];
    incoming_blocks_period_peers_table_.erase(peer_id);
    throw std::logic_error(err.str());
  }

  incoming_blocks_period_peers_table_[peer_id] = period;
}

}  // namespace taraxa::network::tarcap