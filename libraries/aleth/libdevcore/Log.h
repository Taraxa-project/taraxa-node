// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <string>
#include <vector>

#include "FixedHash.h"
#include "Terminal.h"

namespace dev {

// TODO: remove
#define LOG BOOST_LOG
enum Verbosity {
  VerbositySilent = -1,
  VerbosityError = 0,
  VerbosityWarning = 1,
  VerbosityInfo = 2,
  VerbosityDebug = 3,
  VerbosityTrace = 4,
};
struct LoggingOptions {
  int verbosity = VerbosityWarning;
  strings includeChannels;
  strings excludeChannels;
  std::string logfilename;
};

// Below overloads for both const and non-const references are needed, because
// without overload for non-const reference generic
// operator<<(formatting_ostream& _strm, T& _value) will be preferred by
// overload resolution.
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, bigint const& _value) {
  _strm.stream() << EthNavy << _value << EthReset;
  return _strm;
}
// inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, bigint& _value) {
//   auto const& constValue = _value;
//   _strm << constValue;
//   return _strm;
// }

inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, u256 const& _value) {
  _strm.stream() << EthNavy << _value << EthReset;
  return _strm;
}
// inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, u256& _value) {
//   auto const& constValue = _value;
//   _strm << constValue;
//   return _strm;
// }

inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, u160 const& _value) {
  _strm.stream() << EthNavy << _value << EthReset;
  return _strm;
}
// inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, u160& _value) {
//   auto const& constValue = _value;
//   _strm << constValue;
//   return _strm;
// }

template <unsigned N>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, FixedHash<N> const& _value) {
  _strm.stream() << EthTeal "#" << _value.abridged() << EthReset;
  return _strm;
}
template <unsigned N>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, FixedHash<N>& _value) {
  auto const& constValue = _value;
  _strm << constValue;
  return _strm;
}

inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, h160 const& _value) {
  _strm.stream() << EthRed "@" << _value.abridged() << EthReset;
  return _strm;
}
// inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, h160& _value) {
//   auto const& constValue = _value;
//   _strm << constValue;
//   return _strm;
// }

inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, h256 const& _value) {
  _strm.stream() << EthCyan "#" << _value.abridged() << EthReset;
  return _strm;
}
// inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, h256& _value) {
//   auto const& constValue = _value;
//   _strm << constValue;
//   return _strm;
// }

inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, h512 const& _value) {
  _strm.stream() << EthTeal "##" << _value.abridged() << EthReset;
  return _strm;
}
// inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, h512& _value) {
//   auto const& constValue = _value;
//   _strm << constValue;
//   return _strm;
// }

inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, bytesConstRef _value) {
  _strm.stream() << EthYellow "%" << toHex(_value) << EthReset;
  return _strm;
}
}  // namespace dev

// Overloads for types of std namespace can't be defined in dev namespace,
// because they won't be found due to Argument-Dependent Lookup Placing anything
// into std is not allowed, but we can put them into boost::log
namespace boost {
namespace log {
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, dev::bytes const& _value) {
  _strm.stream() << EthYellow "%" << dev::toHex(_value) << EthReset;
  return _strm;
}
// inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, dev::bytes& _value) {
//   auto const& constValue = _value;
//   _strm << constValue;
//   return _strm;
// }

template <typename T>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, std::vector<T> const& _value) {
  _strm.stream() << EthWhite "[ " EthReset;
  int n = 0;
  for (T const& i : _value) {
    _strm.stream() << (n++ ? EthWhite " " EthReset : "");
    _strm << i;
  }
  _strm.stream() << EthWhite " ]" EthReset;
  return _strm;
}
template <typename T>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, std::vector<T>& _value) {
  auto const& constValue = _value;
  _strm << constValue;
  return _strm;
}

template <typename T>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, std::set<T> const& _value) {
  _strm.stream() << EthYellow "{ " EthReset;
  int n = 0;
  for (T const& i : _value) {
    _strm.stream() << (n++ ? EthYellow ", " EthReset : "");
    _strm << i;
  }
  _strm.stream() << EthYellow " }" EthReset;
  return _strm;
}
template <typename T>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, std::set<T>& _value) {
  auto const& constValue = _value;
  _strm << constValue;
  return _strm;
}

template <typename T>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm,
                                                  std::unordered_set<T> const& _value) {
  _strm.stream() << EthYellow "{ " EthReset;
  int n = 0;
  for (T const& i : _value) {
    _strm.stream() << (n++ ? EthYellow ", " EthReset : "");
    _strm << i;
  }
  _strm.stream() << EthYellow " }" EthReset;
  return _strm;
}
template <typename T>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm,
                                                  std::unordered_set<T>& _value) {
  auto const& constValue = _value;
  _strm << constValue;
  return _strm;
}

template <typename T, typename U>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, std::map<T, U> const& _value) {
  _strm.stream() << EthLime "{ " EthReset;
  int n = 0;
  for (auto const& i : _value) {
    _strm << (n++ ? EthLime ", " EthReset : "");
    _strm << i.first;
    _strm << (n++ ? EthLime ": " EthReset : "");
    _strm << i.second;
  }
  _strm.stream() << EthLime " }" EthReset;
  return _strm;
}
template <typename T, typename U>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, std::map<T, U>& _value) {
  auto const& constValue = _value;
  _strm << constValue;
  return _strm;
}

template <typename T, typename U>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm,
                                                  std::unordered_map<T, U> const& _value) {
  _strm << EthLime "{ " EthReset;
  int n = 0;
  for (auto const& i : _value) {
    _strm.stream() << (n++ ? EthLime ", " EthReset : "");
    _strm << i.first;
    _strm.stream() << (n++ ? EthLime ": " EthReset : "");
    _strm << i.second;
  }
  _strm << EthLime " }" EthReset;
  return _strm;
}
template <typename T, typename U>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm,
                                                  std::unordered_map<T, U>& _value) {
  auto const& constValue = _value;
  _strm << constValue;
  return _strm;
}

template <typename T, typename U>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm,
                                                  std::pair<T, U> const& _value) {
  _strm.stream() << EthPurple "( " EthReset;
  _strm << _value.first;
  _strm.stream() << EthPurple ", " EthReset;
  _strm << _value.second;
  _strm.stream() << EthPurple " )" EthReset;
  return _strm;
}
template <typename T, typename U>
inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& _strm, std::pair<T, U>& _value) {
  auto const& constValue = _value;
  _strm << constValue;
  return _strm;
}
}  // namespace log
}  // namespace boost