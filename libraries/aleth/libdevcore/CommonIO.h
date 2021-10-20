// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

/// @file
/// File & stream I/O routines.
#pragma once

#include <array>
#include <boost/filesystem.hpp>
#include <chrono>
#include <iosfwd>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "Common.h"
#include "CommonData.h"

namespace dev {
// Stream I/O functions.
// Provides templated stream I/O for all STL collections so they can be shifted
// on to any iostream-like interface.

template <class T>
struct StreamOut {
  static std::ostream& bypass(std::ostream& _out, T const& _t) {
    _out << _t;
    return _out;
  }
};
template <>
struct StreamOut<uint8_t> {
  static std::ostream& bypass(std::ostream& _out, uint8_t const& _t) {
    _out << (int)_t;
    return _out;
  }
};

inline std::ostream& operator<<(std::ostream& _out, bytes const& _e) {
  _out << toHexPrefixed(_e);
  return _out;
}
template <class T>
inline std::ostream& operator<<(std::ostream& _out, std::vector<T> const& _e);
template <class T, std::size_t Z>
inline std::ostream& operator<<(std::ostream& _out, std::array<T, Z> const& _e);
template <class T, class U>
inline std::ostream& operator<<(std::ostream& _out, std::set<T, U> const& _e);
template <class T, class U>
inline std::ostream& operator<<(std::ostream& _out, std::unordered_set<T, U> const& _e);

#if defined(_WIN32)
template <class T>
inline std::string toString(std::chrono::time_point<T> const& _e, std::string const& _format = "%Y-%m-%d %H:%M:%S")
#else
template <class T>
inline std::string toString(std::chrono::time_point<T> const& _e, std::string const& _format = "%F %T")
#endif
{
  unsigned long milliSecondsSinceEpoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(_e.time_since_epoch()).count();
  auto const durationSinceEpoch = std::chrono::milliseconds(milliSecondsSinceEpoch);
  std::chrono::time_point<std::chrono::system_clock> const tpAfterDuration(durationSinceEpoch);

  tm timeValue;
  auto time = std::chrono::system_clock::to_time_t(tpAfterDuration);
#if defined(_WIN32)
  gmtime_s(&timeValue, &time);
#else
  gmtime_r(&time, &timeValue);
#endif

  unsigned const millisRemainder = milliSecondsSinceEpoch % 1000;
  char buffer[1024];
  if (strftime(buffer, sizeof(buffer), _format.c_str(), &timeValue))
    return std::string(buffer) + "." +
           (millisRemainder < 1     ? "000"
            : millisRemainder < 10  ? "00"
            : millisRemainder < 100 ? "0"
                                    : "") +
           std::to_string(millisRemainder) + "Z";
  return std::string();
}

template <class T>
inline std::ostream& streamout(std::ostream& _out, std::vector<T> const& _e) {
  _out << "[";
  if (!_e.empty()) {
    StreamOut<T>::bypass(_out, _e.front());
    for (auto i = ++_e.begin(); i != _e.end(); ++i) StreamOut<T>::bypass(_out << ",", *i);
  }
  _out << "]";
  return _out;
}

template <class T>
inline std::ostream& operator<<(std::ostream& _out, std::vector<T> const& _e) {
  streamout(_out, _e);
  return _out;
}  // Used in CommonJS.h

template <class T, std::size_t Z>
inline std::ostream& streamout(std::ostream& _out, std::array<T, Z> const& _e)  // used somewhere?
{
  _out << "[";
  if (!_e.empty()) {
    StreamOut<T>::bypass(_out, _e.front());
    auto i = _e.begin();
    for (++i; i != _e.end(); ++i) StreamOut<T>::bypass(_out << ",", *i);
  }
  _out << "]";
  return _out;
}
template <class T, std::size_t Z>
inline std::ostream& operator<<(std::ostream& _out, std::array<T, Z> const& _e) {
  streamout(_out, _e);
  return _out;
}

template <class T>
std::ostream& streamout(std::ostream& _out, std::set<T> const& _v) {
  if (_v.empty()) return _out << "{}";
  int i = 0;
  for (auto p : _v) _out << (!(i++) ? "{ " : ", ") << p;
  return _out << " }";
}
template <class T>
inline std::ostream& operator<<(std::ostream& _out, std::set<T> const& _e) {
  streamout(_out, _e);
  return _out;
}

template <class T>
std::ostream& streamout(std::ostream& _out, std::unordered_set<T> const& _v) {
  if (_v.empty()) return _out << "{}";
  int i = 0;
  for (auto p : _v) _out << (!(i++) ? "{ " : ", ") << p;
  return _out << " }";
}
template <class T>
inline std::ostream& operator<<(std::ostream& _out, std::unordered_set<T> const& _e) {
  streamout(_out, _e);
  return _out;
}

// Functions that use streaming stuff.

/// Converts arbitrary value to string representation using std::stringstream.
template <class _T>
inline std::string toString(_T const& _t) {
  std::ostringstream o;
  o << _t;
  return o.str();
}

template <>
inline std::string toString<std::string>(std::string const& _s) {
  return _s;
}

template <>
inline std::string toString<uint8_t>(uint8_t const& _u) {
  std::ostringstream o;
  o << static_cast<uint16_t>(_u);
  return o.str();
}
}  // namespace dev
