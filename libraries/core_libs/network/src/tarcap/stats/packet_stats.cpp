#include "network/tarcap/stats/packet_stats.hpp"

namespace taraxa::network::tarcap {

std::string PacketStats::getStatsJsonStr(const std::string &packet_type, const dev::p2p::NodeID &node,
                                         bool include_duration_fields) const {
  std::ostringstream ret;
  ret << "{\"type\":\"" << packet_type << "\",";
  ret << "\"size\":" << size_ << ",";
  ret << "\"node\":\"" << node.abridged() << "\"";

  if (include_duration_fields) {
    ret << ",\"processing_duration\":" << processing_duration_.count() << ",";
    ret << "\"tp_wait_duration\":" << tp_wait_duration_.count();
  }

  ret << "}";

  return ret.str();
}

Json::Value PacketStats::getStatsJson(bool include_duration_fields) const {
  Json::Value ret;
  const auto divisor = count_ ? count_ : 1;

  ret["total_count"] = Json::UInt64(count_);
  ret["total_size"] = Json::UInt64(size_);
  ret["avg_size"] = Json::UInt64(size_ / divisor);

  if (include_duration_fields) {
    ret["total_processing_duration"] = Json::UInt64(processing_duration_.count());
    ret["total_tp_wait_duration"] = Json::UInt64(tp_wait_duration_.count());
    ret["avg_processing_duration"] = Json::UInt64(processing_duration_.count() / divisor);
    ret["avg_tp_wait_duration"] = Json::UInt64(tp_wait_duration_.count() / divisor);
  }

  return ret;
}

PacketStats PacketStats::operator-(const PacketStats &ro) const {
  PacketStats result;

  result.count_ = count_ - ro.count_;
  result.size_ = size_ - ro.size_;
  result.processing_duration_ = processing_duration_ - ro.processing_duration_;
  result.tp_wait_duration_ = tp_wait_duration_ - ro.tp_wait_duration_;

  return result;
}

}  // namespace taraxa::network::tarcap