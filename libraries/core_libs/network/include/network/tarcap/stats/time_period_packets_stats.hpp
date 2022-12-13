#pragma once

#include "common/types.hpp"
#include "logger/logger.hpp"
#include "network/tarcap/stats/max_stats.hpp"
#include "network/tarcap/stats/packets_stats.hpp"

namespace taraxa::network::tarcap {

class PeersState;

/**
 * @brief Stats for all received and sent packets of all types
 */
class TimePeriodPacketsStats {
 public:
  TimePeriodPacketsStats(std::chrono::milliseconds reset_time_period, const addr_t& node_addr = {});

  void addReceivedPacket(const std::string& packet_type, const dev::p2p::NodeID& node, const PacketStats& packet);
  void addSentPacket(const std::string& packet_type, const dev::p2p::NodeID& node, const PacketStats& packet);

  /**
   * @brief Logs both received as well as sent packets stats + updates max count/size and reset stats
   */
  void processStats(const std::shared_ptr<PeersState>& peers_state);

  /**
   * @return reset interval [ms] for stats
   */
  uint64_t getResetTimePeriodMs() const;

 private:
  /**
   * @brief Checks if now() - start_time is approximately equal to kResetTimePeriod. Stats are reset in regular
   * interval, but it might be postponed due to lack o resources, in which case we dont want to count such stats when
   * processing max stats
   *
   * @return <true, interval_ms> in case current time is valid time point for max stats ->
   *         now() - start_time == *kResetTimePeriod, otherwise <false, interval_ms>
   */
  std::pair<bool, std::chrono::milliseconds> validMaxStatsTimePeriod(
      const std::chrono::system_clock::time_point& start_time) const;

 private:
  // Interval during which are the peer stats supposed to be collected
  const std::chrono::milliseconds kResetTimePeriod;

  // Collected packets stats during time period
  PacketsStats sent_packets_stats_;
  PacketsStats received_packets_stats_;

  // Max stats for all received packets combined per peer
  MaxStats peer_max_stats_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
