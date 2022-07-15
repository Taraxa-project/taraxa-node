#pragma once

#include <libp2p/Common.h>

#include <atomic>
#include <boost/noncopyable.hpp>

#include "common/util.hpp"

namespace taraxa::network::tarcap {

class TaraxaPeer : public boost::noncopyable {
 public:
  TaraxaPeer()
      : known_dag_blocks_(10000, 1000),
        known_transactions_(500000, 10000),
        known_pbft_blocks_(10000, 1000),
        known_votes_(10000, 1000) {}
  explicit TaraxaPeer(const dev::p2p::NodeID &id)
      : id_(id),
        known_dag_blocks_(10000, 1000),
        known_transactions_(500000, 10000),
        known_pbft_blocks_(10000, 1000),
        known_votes_(100000, 1000) {}

  /**
   * @brief Mark dag block as known
   *
   * @param _hash
   * @return true in case dag block was actually marked as known(was not known before), otherwise false (was already
   * known)
   */
  bool markDagBlockAsKnown(blk_hash_t const &_hash) { return known_dag_blocks_.insert(_hash); }
  bool isDagBlockKnown(blk_hash_t const &_hash) const { return known_dag_blocks_.count(_hash); }

  /**
   * @brief Mark transaction as known
   *
   * @param _hash
   * @return true in case transaction was actually marked as known(was not known before), otherwise false (was already
   * known)
   */
  bool markTransactionAsKnown(trx_hash_t const &_hash) { return known_transactions_.insert(_hash); }
  bool isTransactionKnown(trx_hash_t const &_hash) const { return known_transactions_.count(_hash); }

  // PBFT
  /**
   * @brief Mark vote as known
   *
   * @param _hash
   * @return true in case vote was actually marked as known(was not known before), otherwise false (was already known)
   */
  bool markVoteAsKnown(vote_hash_t const &_hash) { return known_votes_.insert(_hash); }
  bool isVoteKnown(vote_hash_t const &_hash) const { return known_votes_.count(_hash); }

  /**
   * @brief Mark pbft block as known
   *
   * @param _hash
   * @return true in case pbft block was actually marked as known(was not known before), otherwise false (was already
   * known)
   */
  bool markPbftBlockAsKnown(blk_hash_t const &_hash) { return known_pbft_blocks_.insert(_hash); }
  bool isPbftBlockKnown(blk_hash_t const &_hash) const { return known_pbft_blocks_.count(_hash); }

  const dev::p2p::NodeID &getId() const { return id_; }

  /**
   * @brief Reports suspicious pacet
   *
   * @return true in case suspicious packet count within current minute is greater than kMaxSuspiciousPacketPerMinute
   */
  bool reportSuspiciousPacket() {
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

  std::atomic<bool> syncing_ = false;
  std::atomic<uint64_t> dag_level_ = 0;
  std::atomic<uint64_t> pbft_chain_size_ = 0;
  std::atomic<uint64_t> pbft_round_ = 1;
  std::atomic<size_t> pbft_previous_round_next_votes_size_ = 0;
  std::atomic<uint64_t> last_status_pbft_chain_size_ = 0;
  std::atomic_bool peer_dag_synced_ = false;
  std::atomic_bool peer_dag_syncing_ = false;
  std::atomic_bool peer_requested_dag_syncing_ = false;
  std::atomic_bool peer_light_node = false;
  std::atomic<uint64_t> peer_light_node_history = 0;

  // Mutex used to prevent race condition between dag syncing and gossiping
  mutable boost::shared_mutex mutex_for_sending_dag_blocks_;

 private:
  dev::p2p::NodeID id_;

  ExpirationCache<blk_hash_t> known_dag_blocks_;
  ExpirationCache<trx_hash_t> known_transactions_;
  // PBFT
  ExpirationCache<blk_hash_t> known_pbft_blocks_;
  ExpirationCache<vote_hash_t> known_votes_;  // for peers

  std::atomic<uint64_t> timestamp_suspicious_packet_ = 0;
  std::atomic<uint64_t> suspicious_packet_count_ = 0;
  const uint64_t kMaxSuspiciousPacketPerMinute = 1000;
};

}  // namespace taraxa::network::tarcap