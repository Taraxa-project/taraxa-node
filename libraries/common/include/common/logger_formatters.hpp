#pragma once

#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <boost/beast/http/message_fwd.hpp>
#include <libp2p/LoggerFormatters.hpp>

#include "common/types.hpp"

// Logger fmt::formatters for custom types - not part of logger due to circular dependencies

// Specialize fmt::formatter for std::atomic<T>
template <typename T>
struct fmt::formatter<std::atomic<T>> : fmt::formatter<T> {
  template <typename FormatContext>
  auto format(const std::atomic<T>& atomic_val, FormatContext& ctx) const {
    return fmt::formatter<T>::format(atomic_val.load(std::memory_order_relaxed), ctx);
  }
};

// Specialize fmt::formatter for taraxa::blk_hash_t
template <>
struct fmt::formatter<taraxa::blk_hash_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const taraxa::blk_hash_t& val, FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}", val.abridged());
  }
};

// Specialize fmt::formatter for taraxa::addr_t
template <>
struct fmt::formatter<taraxa::addr_t> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(const taraxa::addr_t& val, FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}", val.abridged());
  }
};

// Specialize fmt::formatter for boost::beast::http::request
template <class Body, class Fields>
struct fmt::formatter<boost::beast::http::request<Body, Fields>> : fmt::ostream_formatter {};
