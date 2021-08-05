#include "peers_state.hpp"

namespace taraxa::network::tarcap {

PeersState::PeersState(std::weak_ptr<dev::p2p::Host>&& host) : host_(std::move(host)) {
  auto tmp_host = host_.lock();
  assert(tmp_host);

  node_id_ = tmp_host->id();
}

std::shared_ptr<TaraxaPeer> PeersState::getPeer(const dev::p2p::NodeID& node_id) {
  std::shared_lock lock(peers_mutex_);

  auto it_peer = peers_.find(node_id);
  if (it_peer != peers_.end()) {
    return it_peer->second;
  }

  return nullptr;
}

std::shared_ptr<TaraxaPeer> PeersState::getPendingPeer(const dev::p2p::NodeID& node_id) {
  // TODO: pending_peers_ might have different mutex than peers ?
  std::shared_lock lock(peers_mutex_);

  auto it_peer = pending_peers_.find(node_id);
  if (it_peer != pending_peers_.end()) {
    return it_peer->second;
  }

  return nullptr;
}

std::vector<dev::p2p::NodeID> PeersState::getAllPeersIDs() const {
  std::vector<dev::p2p::NodeID> peers;

  std::shared_lock lock(peers_mutex_);
  std::transform(peers_.begin(), peers_.end(), std::back_inserter(peers),
                 [](std::pair<const dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> const& peer) { return peer.first; });

  return peers;
}

std::vector<dev::p2p::NodeID> PeersState::getAllPendingPeersIDs() const {
  std::vector<dev::p2p::NodeID> peers;

  std::shared_lock lock(peers_mutex_);
  std::transform(pending_peers_.begin(), pending_peers_.end(), std::back_inserter(peers),
                 [](std::pair<const dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> const& peer) { return peer.first; });

  return peers;
}

std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> PeersState::getAllPeers() const {
  std::shared_lock lock(peers_mutex_);
  return std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>>(peers_.begin(), peers_.end());
}

void PeersState::setPendingPeersToReady() {
  std::unique_lock lock(peers_mutex_);
  peers_.merge(pending_peers_);
  pending_peers_.clear();
}

std::shared_ptr<TaraxaPeer> PeersState::addPendingPeer(const dev::p2p::NodeID& node_id) {
  std::unique_lock lock(peers_mutex_);
  auto ret = pending_peers_.emplace(node_id, std::make_shared<TaraxaPeer>(node_id));
  if (!ret.second) {
    // LOG(log_er_) << "Peer " << node_id.abridged() << " is already in pending peers list";
  }

  return ret.first->second;
}

size_t PeersState::getPeersCount() {
  std::shared_lock lock(peers_mutex_);

  return peers_.size();
}

void PeersState::erasePeer(dev::p2p::NodeID const& node_id) {
  std::unique_lock lock(peers_mutex_);
  pending_peers_.erase(node_id);
  peers_.erase(node_id);
}

std::shared_ptr<TaraxaPeer> PeersState::setPeerAsReadyToSendMessages(dev::p2p::NodeID const& node_id,
                                                                     std::shared_ptr<TaraxaPeer> peer) {
  std::unique_lock lock(peers_mutex_);
  pending_peers_.erase(node_id);
  auto ret = peers_.emplace(node_id, std::move(peer));
  if (!ret.second) {
    // LOG(log_er_) << "Peer " << node_id.abridged() << " is already in peers list";
  }

  return ret.first->second;
}

}  // namespace taraxa::network::tarcap