/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-12-14 15:47:31
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-08 17:06:06
 */

#ifndef TYPES_HPP
#define TYPES_HPP
#include <boost/asio.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <chrono>
#include <string>
#include <type_traits>
#include "libdevcore/FixedHash.h"

namespace taraxa {

// time related
using time_point_t = std::chrono::system_clock::time_point;
using time_stamp_t = unsigned long;

using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

// newtork related
using end_point_udp_t = boost::asio::ip::udp::endpoint;
using socket_udp_t = boost::asio::ip::udp::socket;
using resolver_udp_t = boost::asio::ip::udp::resolver;

/**
 * for example, use uint256_t as intermediate type,
 * uint_hash_t <------> uint256_t <------> stringstream  <------> string
 *                      number         encode/decode            operator>>
 */

template <std::size_t Bytes>
struct uint_hash_t {
  static_assert((Bytes == 16 || Bytes == 32 || Bytes == 64),
                "Bytes must be 16, 32 or 64\n");

  using Number = typename std::conditional<
      Bytes == 16, uint128_t,
      typename std::conditional<Bytes == 32, uint256_t, uint512_t>::type>::type;
  uint_hash_t() = default;  // Must be a trivial type for std::is_pod_v<>=true
  uint_hash_t(Number const &number);
  uint_hash_t(std::string const &str);
  uint_hash_t(const char *cstr);
  void encodeHex(std::string &str) const;
  bool decodeHex(std::string const &str);

  Number number() const;
  bool isZero() const;
  void clear();
  void operator=(std::string const &str);
  void operator=(const char *str);
  bool operator==(uint_hash_t const &other) const;
  bool operator<(uint_hash_t const &other) const;
  bool operator>(uint_hash_t const &other) const;

  std::string toString() const;

  struct hash {
    size_t operator()(uint_hash_t<Bytes> const &value) const {
      return boost::hash_range(value.bytes.cbegin(), value.bytes.cend());
    }
  };

  // debugging
  void rawPrint() const;
  std::array<uint8_t, Bytes> bytes;
};

template <std::size_t Byte>
std::ostream &operator<<(std::ostream &strm, uint_hash_t<Byte> const &num);

using uint256_hash_t = dev::FixedHash<32>;
using uint512_hash_t = dev::FixedHash<64>;
using uint520_hash_t = dev::FixedHash<65>;
using uint160_hash_t = dev::FixedHash<20>;

using secret_t = dev::SecureFixedHash<32>;
using public_t = uint512_hash_t;
using addr_t = uint160_hash_t;
using sig_t = uint520_hash_t;
using sig_hash_t = uint256_hash_t;
using blk_hash_t = uint256_hash_t;
using trx_hash_t = uint256_hash_t;

using key_t = std::string;
using bal_t = uint64_t; // Use uint64_t for balance as in Taraxa
using val_t = uint256_hash_t;

using vec_blk_t = std::vector<blk_hash_t>;
using vec_trx_t = std::vector<trx_hash_t>;
using byte = uint8_t;
using bytes = std::vector<byte>;
using node_id_t = uint512_hash_t;

std::ostream &operator<<(std::ostream &strm, bytes const &bytes);

time_point_t getLong2TimePoint(unsigned long l);
unsigned long getTimePoint2Long(time_point_t tp);

bytes str2bytes(std::string const &str);
std::string bytes2str(bytes const &data);

}  // namespace taraxa

namespace std {
// Forward std::hash<taraxa::uint_hash_t> to taraxa::uint_hash_t::hash
// template <>
// struct hash<taraxa::blk_hash_t> : taraxa::blk_hash_t::hash {};
// template <>
// struct hash<taraxa::sig_t> : taraxa::sig_t::hash {};

}  // namespace std

#endif
