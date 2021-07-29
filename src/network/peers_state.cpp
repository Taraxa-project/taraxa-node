#include "peers_state.hpp"

namespace taraxa {

std::mt19937_64 PeersState::urng_{std::random_device()()};

std::shared_ptr<TaraxaPeer> PeersState::getPeer(dev::p2p::NodeID const &node_id) const {
  std::shared_lock lock(peers_mutex_);
  auto itPeer = peers_.find(node_id);
  if (itPeer != peers_.end()) {
    return itPeer->second;
  }
  return nullptr;
}

std::shared_ptr<TaraxaPeer> PeersState::getPendingPeer(dev::p2p::NodeID const &node_id) const {
  std::shared_lock lock(peers_mutex_);
  auto itPeer = pending_peers_.find(node_id);
  if (itPeer != pending_peers_.end()) {
    return itPeer->second;
  }
  return nullptr;
}

size_t PeersState::getPeersCount() const {
  std::shared_lock lock(peers_mutex_);
  return peers_.size();
}

void PeersState::erasePeer(dev::p2p::NodeID const &node_id) {
  std::unique_lock lock(peers_mutex_);
  pending_peers_.erase(node_id);
  peers_.erase(node_id);
}

std::shared_ptr<TaraxaPeer> PeersState::addPendingPeer(dev::p2p::NodeID const &node_id) {
  std::unique_lock lock(peers_mutex_);
  auto ret = pending_peers_.emplace(node_id, std::make_shared<TaraxaPeer>(node_id));
  if (!ret.second) {
    // TODO: keep error level until we know exactly when and why is this happening
    LOG(log_er_) << "Peer " << node_id.abridged() << " is already in pending peers list";
  }

  return ret.first->second;
}

std::shared_ptr<TaraxaPeer> PeersState::setPeerAsReadyToSendMessages(dev::p2p::NodeID const &node_id,
                                                                     std::shared_ptr<TaraxaPeer> peer) {
  std::unique_lock lock(peers_mutex_);
  pending_peers_.erase(node_id);
  auto ret = peers_.emplace(node_id, peer);
  if (!ret.second) {
    // TODO: keep error level until we know exactly when and why is this happening
    LOG(log_er_) << "Peer " << node_id.abridged() << " is already in peers list";
  }

  return ret.first->second;
}

std::vector<dev::p2p::NodeID> PeersState::selectPeers(std::function<bool(TaraxaPeer const &)> const &_predicate) const {
  std::vector<dev::p2p::NodeID> allowed;
  std::shared_lock lock(peers_mutex_);
  for (auto const &peer : peers_) {
    if (_predicate(*peer.second)) allowed.push_back(peer.first);
  }
  return allowed;
}

std::vector<dev::p2p::NodeID> PeersState::getAllPeersIDs() const {
  std::vector<dev::p2p::NodeID> peers;

  std::shared_lock lock(peers_mutex_);
  peers.reserve(peers_.size());
  std::transform(
      peers_.begin(), peers_.end(), std::back_inserter(peers),
      [](std::pair<const dev::p2p::NodeID, std::shared_ptr<taraxa::TaraxaPeer>> const &peer) { return peer.first; });

  return peers;
}

std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> PeersState::getAllPeers() const {
  std::shared_lock lock(peers_mutex_);
  return std::unordered_map<dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>>(peers_.begin(), peers_.end());
}

std::vector<dev::p2p::NodeID> PeersState::getAllPendingPeers() const {
  std::vector<dev::p2p::NodeID> peers;

  std::shared_lock lock(peers_mutex_);
  peers.reserve(pending_peers_.size());
  std::transform(
      pending_peers_.begin(), pending_peers_.end(), std::back_inserter(peers),
      [](std::pair<const dev::p2p::NodeID, std::shared_ptr<taraxa::TaraxaPeer>> const &peer) { return peer.first; });

  return peers;
}

std::pair<std::vector<dev::p2p::NodeID>, std::vector<dev::p2p::NodeID>> PeersState::randomPartitionPeers(
    std::vector<dev::p2p::NodeID> const &_peers, std::size_t _number) {
  std::vector<dev::p2p::NodeID> part1(_peers);
  std::vector<dev::p2p::NodeID> part2;

  if (_number >= _peers.size()) return std::make_pair(part1, part2);

  std::shuffle(part1.begin(), part1.end(), urng_);

  // Remove elements from the end of the shuffled part1 vector and move
  // them to part2.
  std::move(part1.begin() + _number, part1.end(), std::back_inserter(part2));
  part1.erase(part1.begin() + _number, part1.end());
  return std::make_pair(move(part1), move(part2));
}

}  // namespace taraxa