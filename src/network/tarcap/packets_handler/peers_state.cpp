#include "peers_state.hpp"

namespace taraxa::network::tarcap {

PeersState::PeersState(std::weak_ptr<dev::p2p::Host>&& host) : host_(std::move(host)) {
  auto tmp_host = host_.lock();
  assert(tmp_host);

  node_id_ = tmp_host->id();
}

std::shared_ptr<TaraxaPeer> PeersState::getPeer(const dev::p2p::NodeID& node_id) {
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);

  auto itPeer = peers_.find(node_id);
  if (itPeer != peers_.end()) {
    return itPeer->second;
  }

  return nullptr;
}

std::shared_ptr<TaraxaPeer> PeersState::getPendingPeer(const dev::p2p::NodeID& node_id) {
  // TODO: pending_peers_ might have different mutex than peers ?
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);

  auto itPeer = pending_peers_.find(node_id);
  if (itPeer != pending_peers_.end()) {
    return itPeer->second;
  }

  return nullptr;
}

std::shared_ptr<TaraxaPeer> PeersState::addPendingPeer(const dev::p2p::NodeID& node_id) {
  boost::unique_lock<boost::shared_mutex> lock(peers_mutex_);
  auto ret = pending_peers_.emplace(node_id, std::make_shared<TaraxaPeer>(node_id));
  if (!ret.second) {
    // LOG(log_er_) << "Peer " << node_id.abridged() << " is already in pending peers list";
  }

  return ret.first->second;
}

size_t PeersState::getPeersCount() {
  boost::shared_lock<boost::shared_mutex> lock(peers_mutex_);

  return peers_.size();
}

void PeersState::erasePeer(dev::p2p::NodeID const& node_id) {
  boost::unique_lock<boost::shared_mutex> lock(peers_mutex_);
  pending_peers_.erase(node_id);
  peers_.erase(node_id);
}

std::shared_ptr<TaraxaPeer> PeersState::setPeerAsReadyToSendMessages(dev::p2p::NodeID const& node_id,
                                                                     std::shared_ptr<TaraxaPeer> peer) {
  boost::unique_lock<boost::shared_mutex> lock(peers_mutex_);
  pending_peers_.erase(node_id);
  auto ret = peers_.emplace(node_id, std::move(peer));
  if (!ret.second) {
    // LOG(log_er_) << "Peer " << node_id.abridged() << " is already in peers list";
  }

  return ret.first->second;
}

bool PeersState::sealAndSend(const dev::p2p::NodeID& nodeID, SubprotocolPacketType packet_type, dev::RLPStream rlp) {
  // TODO: is weak_ptr->lock() threadsafe ???
  auto host = host_.lock();
  if (!host) {
    // TODO: process somehow logs - maybe create global generic logger ???
    // LOG(log_er_) << "sealAndSend failed to obtain host";
    return false;
  }

  auto peer = getPeer(nodeID);
  if (!peer) {
    peer = getPendingPeer(nodeID);

    if (!peer) {
      // LOG(log_er_) << "sealAndSend failed to find peer";
      return false;
    }

    if (packet_type != SubprotocolPacketType::StatusPacket) {
      // LOG(log_er_) << "sealAndSend failed initial status check, peer " << nodeID.abridged() << " will be
      // disconnected";
      host->disconnect(nodeID, dev::p2p::UserReason);
      return false;
    }
  }

  auto packet_size = rlp.out().size();

  // This situation should never happen - packets bigger than 16MB cannot be sent due to networking layer limitations.
  // If we are trying to send packets bigger than that, it should be split to multiple packets
  // or handled in some other way in high level code - e.g. function that creates such packet and calls sealAndSend
  if (packet_size > MAX_PACKET_SIZE) {
    //    LOG(log_er_) << "Trying to send packet bigger than PACKET_MAX_SIZE(" << MAX_PACKET_SIZE << ") - rejected !"
    //                 << " Packet type: " << convertPacketTypeToString(packet_type) << ", size: " << packet_size
    //                 << ", receiver: " << nodeID.abridged();
    return false;
  }

  // TODO: replace hardcoded "taraxa" by some common function that is also used in overridden tarcap class name()
  //       function
  host->send(nodeID, "taraxa", packet_type, move(rlp.invalidate()));

  SinglePacketStats packet_stats{nodeID, packet_size, false, std::chrono::microseconds{0},
                                 std::chrono::microseconds{0}};
  packets_stats_.addSentPacket(node_id_, convertPacketTypeToString(packet_type), packet_stats);

  return true;
}

}  // namespace taraxa::network::tarcap