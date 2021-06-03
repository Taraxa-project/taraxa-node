#pragma once

#include <libdevcore/Address.h>
#include <libdevcore/FixedHash.h>
#include <libdevcrypto/Common.h>

#include <boost/asio.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <chrono>
#include <filesystem>
#include <string>
#include <type_traits>

namespace taraxa {

namespace fs = std::filesystem;

using dev::Address;
using dev::AddressSet;
using dev::bytes;
using dev::bytesConstRef;
using dev::h256;
using dev::h256Hash;
using dev::h256s;
using dev::h64;
using dev::Secret;
using dev::u256;

using EthBlockNumber = uint64_t;

// time related
using time_point_t = std::chrono::system_clock::time_point;
using time_stamp_t = unsigned long;

using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;
using uint1024_t = boost::multiprecision::uint1024_t;

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
  static_assert((Bytes == 16 || Bytes == 32 || Bytes == 64), "Bytes must be 16, 32 or 64\n");

  using Number = typename std::conditional<Bytes == 16, uint128_t,
                                           typename std::conditional<Bytes == 32, uint256_t, uint512_t>::type>::type;
  uint_hash_t() = default;  // Must be a trivial type for std::is_pod_v<>=true
  explicit uint_hash_t(Number const &number);
  explicit uint_hash_t(std::string const &str);
  explicit uint_hash_t(const char *cstr);
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

using uint256_t = boost::multiprecision::uint256_t;
using uint512_t = boost::multiprecision::uint512_t;

using uint256_hash_t = dev::FixedHash<32>;
using uint512_hash_t = dev::FixedHash<64>;
using uint520_hash_t = dev::FixedHash<65>;
using uint160_hash_t = dev::FixedHash<20>;

using secret_t = dev::SecureFixedHash<32>;
using public_t = uint512_hash_t;
using addr_t = uint160_hash_t;
using sig_t = uint520_hash_t;
using sig_hash_t = uint256_hash_t;
using vote_hash_t = uint256_hash_t;
using blk_hash_t = uint256_hash_t;
using trx_hash_t = uint256_hash_t;
using sig_hash_t = uint256_hash_t;

using gas_t = uint64_t;
using key_t = std::string;
using level_t = uint64_t;
using val_t = dev::u256;
using root_t = dev::h256;
using dag_blk_num_t = uint64_t;

using vec_blk_t = std::vector<blk_hash_t>;
using vec_trx_t = std::vector<trx_hash_t>;
using trx_num_t = vec_trx_t::size_type;
using byte = uint8_t;
using bytes = std::vector<byte>;
using node_id_t = uint512_hash_t;
using round_t = uint64_t;
using trx_nonce_t = val_t;

// val_t type related helper functions
inline val_t operator+=(val_t const &val, val_t const &other) { return val + other; }
inline std::string toString(val_t const &val) {
  std::stringstream strm;
  strm << val;
  return strm.str();
}

std::ostream &operator<<(std::ostream &strm, bytes const &bytes);

time_point_t getLong2TimePoint(unsigned long l);
unsigned long getTimePoint2Long(time_point_t tp);

bytes str2bytes(std::string const &str);
std::string bytes2str(bytes const &data);

}  // namespace taraxa
