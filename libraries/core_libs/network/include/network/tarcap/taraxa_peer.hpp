#pragma once

#include <libp2p/Common.h>

#include <atomic>
#include <boost/noncopyable.hpp>

#include "common/util.hpp"
#include "network/tarcap/stats/packets_stats.hpp"

namespace taraxa::network::tarcap {

class TaraxaPeer : public boost::noncopyable {
 public:
  TaraxaPeer();
  TaraxaPeer(const dev::p2p::NodeID& id, size_t transaction_pool_size);

  /**
   * @brief Mark dag block as known
   *
   * @param _hash
   * @return true in case dag block was actually marked as known(was not known before), otherwise false (was already
   * known)
   */
  bool markDagBlockAsKnown(const blk_hash_t& hash);
  bool isDagBlockKnown(const blk_hash_t& hash) const;

  /**
   * @brief Mark transaction as known
   *
   * @param _hash
   * @return true in case transaction was actually marked as known(was not known before), otherwise false (was already
   * known)
   */
  bool markTransactionAsKnown(const trx_hash_t& hash);
  bool isTransactionKnown(const trx_hash_t& hash) const;

  /**
   * @brief Mark vote as known
   *
   * @param _hash
   * @return true in case vote was actually marked as known(was not known before), otherwise false (was already known)
   */
  bool markVoteAsKnown(const vote_hash_t& hash);
  bool isVoteKnown(const vote_hash_t& hash) const;

  /**
   * @brief Mark pbft block as known
   *
   * @param _hash
   * @return true in case pbft block was actually marked as known(was not known before), otherwise false (was already
   * known)
   */
  bool markPbftBlockAsKnown(const blk_hash_t& hash);
  bool isPbftBlockKnown(const blk_hash_t& hash) const;

  const dev::p2p::NodeID& getId() const;

  /**
   * @brief Reports suspicious pacet
   *
   * @return true in case suspicious packet count within current minute is greater than kMaxSuspiciousPacketPerMinute
   */
  bool reportSuspiciousPacket();

  /**
   * @brief Check if it is allowed to send dag syncing request
   *
   * @return true if allowed
   */
  bool dagSyncingAllowed() const;

  /**
   * @brief Check if it is allowed to receive dag syncing request
   *
   * @return true if allowed
   */
  bool requestDagSyncingAllowed() const;

  /**
   * @brief Adds packet to the stats
   *
   * @param packet_type
   * @param packet
   */
  void addSentPacket(const std::string& packet_type, const PacketStats& packet);

  /**
   * @return AllPacketsStats - packets stats for packets received from peer
   */
  std::pair<std::chrono::system_clock::time_point, PacketStats> getAllPacketsStatsCopy() const;

  /**
   * @brief Resets sent packets stats
   */
  void resetPacketsStats();

 public:
  std::atomic<bool> syncing_ = false;
  std::atomic<uint64_t> dag_level_ = 0;
  std::atomic<PbftPeriod> pbft_chain_size_ = 0;
  std::atomic<PbftPeriod> pbft_period_ = pbft_chain_size_ = 1;
  std::atomic<PbftRound> pbft_round_ = 1;
  std::atomic<size_t> pbft_previous_round_next_votes_size_ = 0;
  std::atomic<PbftPeriod> last_status_pbft_chain_size_ = 0;
  std::atomic_bool peer_dag_synced_ = false;
  std::atomic_uint64_t peer_dag_synced_time_ = 0;
  std::atomic_bool peer_dag_syncing_ = false;
  std::atomic_bool peer_requested_dag_syncing_ = false;
  std::atomic_uint64_t peer_requested_dag_syncing_time_ = 0;
  std::atomic_bool peer_light_node = false;
  std::atomic<PbftPeriod> peer_light_node_history = 0;

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

  // Performance extensive dag syncing is only allowed to be requested once each kDagSyncingLimit seconds
  const uint64_t kDagSyncingLimit = 300;

  // Packets stats for packets sent by *this TaraxaPeer
  PacketsStats sent_packets_stats_;
};

}  // namespace taraxa::network::tarcap