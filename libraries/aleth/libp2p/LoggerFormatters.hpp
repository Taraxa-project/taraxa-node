#pragma once

#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include "Common.h"
#include "ENR.h"

template <typename T>
struct BoostOstreamFormatter : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const T& val, FormatContext& ctx) const {
    std::string str;
    boost::log::formatting_ostream os(str);
    os << val;
    return fmt::formatter<std::string>::format(str, ctx);
  }
};

// Specialize fmt::formatter for PeerSessionInfo
template <>
struct fmt::formatter<dev::p2p::PeerSessionInfo> : BoostOstreamFormatter<dev::p2p::PeerSessionInfo> {};

// Specialize fmt::formatter for Node
template <>
struct fmt::formatter<dev::p2p::Node> : BoostOstreamFormatter<dev::p2p::Node> {};

// Specialize fmt::formatter for bi::tcp::endpoint
template <>
struct fmt::formatter<bi::tcp::endpoint> : BoostOstreamFormatter<bi::tcp::endpoint> {};

// Specialize fmt::formatter for bi::tcp::endpoint
template <>
struct fmt::formatter<bi::udp::endpoint> : BoostOstreamFormatter<bi::udp::endpoint> {};

// Specialize fmt::formatter for dev::p2p::NodeIPEndpoint
template <>
struct fmt::formatter<dev::p2p::NodeIPEndpoint> : fmt::ostream_formatter {};

// Specialize fmt::formatter for dev::p2p::NodeID
template <>
struct fmt::formatter<dev::p2p::NodeID> : fmt::ostream_formatter {};

// Specialize fmt::formatter for ENR
template <>
struct fmt::formatter<dev::p2p::ENR> : fmt::ostream_formatter {};
