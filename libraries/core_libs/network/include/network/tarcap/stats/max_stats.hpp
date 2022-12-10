#pragma once

#include "network/tarcap/stats/packets_stats.hpp"

namespace taraxa::network::tarcap {

///**
// * @brief Stats for all received (or sent) packets of all types
// */
// class MaxStats {
// public:
//
// private:
//  // Time point since which are the stats measured
//  std::chrono::system_clock::time_point start_time_;
//
//  // Stats for all packets types combined
//  AllPacketsStats::Stats all_packets_stats_;
//
//  // Stas per individual packet type
//  PerPacketStatsMap per_packet_stats_;
//  mutable std::shared_mutex mutex_;
//
//  // TODO: maybe put max stats into separate class together with max stats per peer ?
//  // Statistics about max number of packets (of the same type) received during fixed time period
//  std::unordered_map<std::string /*packet name*/, Stats> max_counts_stats_;
//  // Statistics about max size of packets (of the same type) received during fixed time period
//  std::unordered_map<std::string /*packet name*/, Stats> max_sizes_stats_;
//  mutable std::shared_mutex max_stats_mutex_;
//};
//
// AllPacketsStats::PerPacketStatsMap operator-(const AllPacketsStats::PerPacketStatsMap &lo,
//                                             const AllPacketsStats::PerPacketStatsMap &ro);

}  // namespace taraxa::network::tarcap
