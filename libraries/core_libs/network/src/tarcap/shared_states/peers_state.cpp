#include "network/tarcap/shared_states/peers_state.hpp"

namespace taraxa::network::tarcap {

PeersState::PeersState(std::weak_ptr<dev::p2p::Host> host, const FullNodeConfig& conf)
    : host_(std::move(host)), kConf(conf) {}

std::shared_ptr<TaraxaPeer> PeersState::getPeer(const dev::p2p::NodeID& node_id) const {
  std::shared_lock lock(peers_mutex_);

  auto it_peer = peers_.find(node_id);
  if (it_peer != peers_.end()) {
    return it_peer->second;
  }

  return nullptr;
}

std::shared_ptr<TaraxaPeer> PeersState::getPendingPeer(const dev::p2p::NodeID& node_id) const {
  std::shared_lock lock(peers_mutex_);

  auto it_peer = pending_peers_.find(node_id);
  if (it_peer != pending_peers_.end()) {
    return it_peer->second;
  }

  return nullptr;
}

std::pair<std::shared_ptr<TaraxaPeer>, std::string> PeersState::getPacketSenderPeer(
    const dev::p2p::NodeID& node_id, SubprotocolPacketType packet_type) const {
  std::shared_lock lock(peers_mutex_);

  // If peer is in peers_, it means he already sent us initial status packet and
  // we can receive/send any kind of packet from/to him
  if (const auto it_peer = peers_.find(node_id); it_peer != peers_.end()) {
    return {it_peer->second, ""};
  }

  // If peer is in pending_peers_, it means he has not yet sent us initial status packet and
  // we can receive/send only StatusPacket from/to him
  if (const auto it_peer = pending_peers_.find(node_id); it_peer != pending_peers_.end()) {
    if (packet_type == SubprotocolPacketType::StatusPacket) {
      return {it_peer->second, ""};
    } else {
      std::ostringstream error;
      error << "Peer " << node_id.abridged()
            << " is only in pending peers - probably did not send initial status packet yet";
      return {nullptr, error.str()};
    }
  }

  std::ostringstream error;
  error << "Peer " << node_id.abridged() << " is not in peers map anymore - probably lost connection";
  return {nullptr, error.str()};
}

std::vector<dev::p2p::NodeID> PeersState::getAllPendingPeersIDs() const {
  std::vector<dev::p2p::NodeID> peers;

  std::shared_lock lock(peers_mutex_);
  peers.reserve(pending_peers_.size());
  std::transform(pending_peers_.begin(), pending_peers_.end(), std::back_inserter(peers),
                 [](std::pair<const dev::p2p::NodeID, std::shared_ptr<TaraxaPeer>> const& peer) { return peer.first; });

  return peers;
}

PeersState::PeersMap PeersState::getAllPeers() const {
  std::shared_lock lock(peers_mutex_);
  return peers_;
}

std::shared_ptr<TaraxaPeer> PeersState::addPendingPeer(const dev::p2p::NodeID& node_id, const std::string& address) {
  std::unique_lock lock(peers_mutex_);
  auto ret =
      pending_peers_.emplace(node_id, std::make_shared<TaraxaPeer>(node_id, kConf.transactions_pool_size, address));
  if (!ret.second) {
    // LOG(log_er_) << "Peer " << node_id.abridged() << " is already in pending peers list";
  }

  return ret.first->second;
}

size_t PeersState::getPeersCount() const {
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

void PeersState::set_peer_malicious(const dev::p2p::NodeID& peer_id) {
  malicious_peers_.emplace(peer_id, std::chrono::steady_clock::now());
}

bool PeersState::is_peer_malicious(const dev::p2p::NodeID& peer_id) {
  if (kConf.network.disable_peer_blacklist) {
    return false;
  }

  // Peers are marked malicious for the time defined in conf_.peer_blacklist_timeout
  if (auto i = malicious_peers_.get(peer_id); i.second) {
    if (kConf.network.peer_blacklist_timeout == 0 ||
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - i.first).count() <=
            kConf.network.peer_blacklist_timeout) {
      return true;
    } else {
      malicious_peers_.erase(peer_id);
    }
  }

  // Delete any expired item from the list
  if (kConf.network.peer_blacklist_timeout > 0) {
    malicious_peers_.erase([this](const std::chrono::steady_clock::time_point& value) {
      return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - value).count() >
             kConf.network.peer_blacklist_timeout;
    });
  }

  return false;
}

}  // namespace taraxa::network::tarcap