// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2013-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

/// @file
/// Very common stuff (i.e. that every other header needs except vector_ref.h).
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#pragma warning(push)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma warning(pop)
#pragma GCC diagnostic pop
#include "vector_ref.h"

// CryptoPP defines byte in the global namespace, so must we.
using byte = uint8_t;

#define DEV_IGNORE_EXCEPTIONS(X) \
  try {                          \
    X;                           \
  } catch (...) {                \
  }

#define DEV_IF_THROWS(X) \
  try {                  \
    X;                   \
  } catch (...)

namespace dev {
using namespace boost::multiprecision::literals;

extern char const* Version;

extern std::string const EmptyString;

// Binary data types.
using bytes = std::vector<::byte>;
using bytesRef = vector_ref<::byte>;
using bytesConstRef = vector_ref<::byte const>;

template <class T>
class secure_vector {
 public:
  secure_vector() {}
  secure_vector(secure_vector<T> const& /*_c*/) = default;  // See https://github.com/ethereum/libweb3core/pull/44
  explicit secure_vector(size_t _size) : m_data(_size) {}
  explicit secure_vector(size_t _size, T _item) : m_data(_size, _item) {}
  explicit secure_vector(std::vector<T> const& _c) : m_data(_c) {}
  explicit secure_vector(vector_ref<T> _c) : m_data(_c.data(), _c.data() + _c.size()) {}
  explicit secure_vector(vector_ref<const T> _c) : m_data(_c.data(), _c.data() + _c.size()) {}
  ~secure_vector() { ref().cleanse(); }

  secure_vector<T>& operator=(secure_vector<T> const& _c) {
    if (&_c == this) return *this;

    ref().cleanse();
    m_data = _c.m_data;
    return *this;
  }
  std::vector<T>& writable() {
    clear();
    return m_data;
  }
  std::vector<T> const& makeInsecure() const { return m_data; }

  void clear() { ref().cleanse(); }

  vector_ref<T> ref() { return vector_ref<T>(&m_data); }
  vector_ref<T const> ref() const { return vector_ref<T const>(&m_data); }

  size_t size() const { return m_data.size(); }
  bool empty() const { return m_data.empty(); }

  void swap(secure_vector<T>& io_other) { m_data.swap(io_other.m_data); }

 private:
  std::vector<T> m_data;
};

using bytesSec = secure_vector<::byte>;

// Numeric types.
using bigint = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>>;
using u64 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    64, 64, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using u128 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    128, 128, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using u256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using s256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    256, 256, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using u160 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    160, 160, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using s160 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    160, 160, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using u512 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    512, 512, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using s512 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    512, 512, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using u256s = std::vector<u256>;
using u160s = std::vector<u160>;
using u256Set = std::set<u256>;
using u160Set = std::set<u160>;

// Map types.
using StringMap = std::map<std::string, std::string>;
using BytesMap = std::map<bytes, bytes>;
using u256Map = std::map<u256, u256>;
using HexMap = std::map<bytes, bytes>;

// Hash types.
using StringHashMap = std::unordered_map<std::string, std::string>;
using u256HashMap = std::unordered_map<u256, u256>;

// String types.
using strings = std::vector<std::string>;

// Null/Invalid values for convenience.
extern bytes const NullBytes;

template <size_t n>
inline u256 exp10() {
  return exp10<n - 1>() * u256(10);
}

template <>
inline u256 exp10<0>() {
  return u256(1);
}

/// RAII utility class whose destructor calls a given function.
class ScopeGuard {
 public:
  ScopeGuard(std::function<void(void)> _f) : m_f(_f) {}
  ~ScopeGuard() { m_f(); }

 private:
  std::function<void(void)> m_f;
};

/// Simple scope-based timer helper.
class TimerHelper {
 public:
  TimerHelper(std::string const& _id, unsigned _msReportWhenGreater = 0)
      : m_t(std::chrono::high_resolution_clock::now()), m_id(_id), m_ms(_msReportWhenGreater) {}
  ~TimerHelper();

 private:
  std::chrono::high_resolution_clock::time_point m_t;
  std::string m_id;
  unsigned m_ms;
};

class Timer {
 public:
  Timer() { restart(); }

  std::chrono::high_resolution_clock::duration duration() const {
    return std::chrono::high_resolution_clock::now() - m_t;
  }
  double elapsed() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(duration()).count() / 1000000.0;
  }
  void restart() { m_t = std::chrono::high_resolution_clock::now(); }

 private:
  std::chrono::high_resolution_clock::time_point m_t;
};

#define DEV_TIMED(S) \
  for (::std::pair<::dev::TimerHelper, bool> __eth_t(S, true); __eth_t.second; __eth_t.second = false)
#define DEV_TIMED_SCOPE(S) ::dev::TimerHelper __eth_t(S)
#if defined(_WIN32)
#define DEV_TIMED_FUNCTION DEV_TIMED_SCOPE(__FUNCSIG__)
#else
#define DEV_TIMED_FUNCTION DEV_TIMED_SCOPE(__PRETTY_FUNCTION__)
#endif

#define DEV_TIMED_ABOVE(S, MS)                                                                         \
  for (::std::pair<::dev::TimerHelper, bool> __eth_t(::dev::TimerHelper(S, MS), true); __eth_t.second; \
       __eth_t.second = false)
#define DEV_TIMED_SCOPE_ABOVE(S, MS) ::dev::TimerHelper __eth_t(S, MS)
#if defined(_WIN32)
#define DEV_TIMED_FUNCTION_ABOVE(MS) DEV_TIMED_SCOPE_ABOVE(__FUNCSIG__, MS)
#else
#define DEV_TIMED_FUNCTION_ABOVE(MS) DEV_TIMED_SCOPE_ABOVE(__PRETTY_FUNCTION__, MS)
#endif

#define DEV_UNUSED __attribute__((unused))

/// Get the current time in seconds since the epoch in UTC
int64_t utcTime();

static const auto c_steadyClockMin = std::chrono::steady_clock::time_point::min();

}  // namespace dev
