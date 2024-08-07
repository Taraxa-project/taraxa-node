#include "network/tarcap/taraxa_peer.hpp"

namespace taraxa::network::tarcap {

TaraxaPeer::TaraxaPeer()
    : known_dag_blocks_(10000, 1000, 10),
      known_transactions_(100000, 10000, 10),
      known_pbft_blocks_(10000, 1000, 10),
      known_votes_(10000, 1000, 10) {}

TaraxaPeer::TaraxaPeer(const dev::p2p::NodeID& id, size_t transaction_pool_size, std::string address)
    : address_(address),
      id_(id),
      known_dag_blocks_(10000, 1000, 10),
      known_transactions_(transaction_pool_size * 1.2, transaction_pool_size / 10, 10),
      known_pbft_blocks_(10000, 1000, 10),
      known_votes_(10000, 1000, 10) {}

bool TaraxaPeer::markDagBlockAsKnown(const blk_hash_t& hash) {
  return known_dag_blocks_.insert(hash, pbft_chain_size_);
}

bool TaraxaPeer::isDagBlockKnown(const blk_hash_t& hash) const { return known_dag_blocks_.contains(hash); }

bool TaraxaPeer::markTransactionAsKnown(const trx_hash_t& hash) {
  return known_transactions_.insert(hash, pbft_chain_size_);
}

bool TaraxaPeer::isTransactionKnown(const trx_hash_t& hash) const { return known_transactions_.contains(hash); }

bool TaraxaPeer::markPbftVoteAsKnown(const vote_hash_t& hash) { return known_votes_.insert(hash, pbft_chain_size_); }

bool TaraxaPeer::isPbftVoteKnown(const vote_hash_t& hash) const { return known_votes_.contains(hash); }

bool TaraxaPeer::markPbftBlockAsKnown(const blk_hash_t& hash) {
  return known_pbft_blocks_.insert(hash, pbft_chain_size_);
}

bool TaraxaPeer::isPbftBlockKnown(const blk_hash_t& hash) const { return known_pbft_blocks_.contains(hash); }

bool TaraxaPeer::markPillarVoteAsKnown(const vote_hash_t& hash) { return known_votes_.insert(hash, pbft_chain_size_); }

bool TaraxaPeer::isPillarVoteKnown(const vote_hash_t& hash) const { return known_votes_.contains(hash); }

const dev::p2p::NodeID& TaraxaPeer::getId() const { return id_; }

bool TaraxaPeer::reportSuspiciousPacket() {
  uint64_t now =
      std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch()).count();
  // Reset counter if no suspicious packet sent for over a minute
  if (now > timestamp_suspicious_packet_) {
    suspicious_packet_count_ = 1;
    timestamp_suspicious_packet_ = now;
  } else {
    suspicious_packet_count_++;
  }
  return suspicious_packet_count_ > kMaxSuspiciousPacketPerMinute;
}

bool TaraxaPeer::dagSyncingAllowed() const {
  return !peer_dag_synced_ ||
         (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
              .count() -
          peer_dag_synced_time_) > kDagSyncingLimit;
}

bool TaraxaPeer::requestDagSyncingAllowed() const {
  return !peer_requested_dag_syncing_ ||
         (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
              .count() -
          peer_requested_dag_syncing_time_) > kDagSyncingLimit;
}

void TaraxaPeer::addSentPacket(const std::string& packet_type, const PacketStats& packet) {
  sent_packets_stats_.addPacket(packet_type, packet);
}

std::pair<std::chrono::system_clock::time_point, PacketStats> TaraxaPeer::getAllPacketsStatsCopy() const {
  return sent_packets_stats_.getAllPacketsStatsCopy();
}

void TaraxaPeer::resetPacketsStats() { sent_packets_stats_.resetStats(); }

void TaraxaPeer::resetKnownCaches() {
  known_transactions_.clear();
  known_dag_blocks_.clear();
  known_votes_.clear();
  known_pbft_blocks_.clear();
}

}  // namespace taraxa::network::tarcap