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
using trx_nonce_t = val_t;

// val_t type related helper functions
inline val_t operator+=(val_t const &val, val_t const &other) { return val + other; }
inline std::string toString(val_t const &val) {
  std::stringstream strm;
  strm << val;
  return strm.str();
}

inline bytes str2bytes(std::string const &str) {
  assert(str.size() % 2 == 0);
  bytes data;
  // convert it to byte stream
  for (size_t i = 0; i < str.size(); i += 2) {
    std::string s = str.substr(i, 2);
    auto t = std::stoul(s, nullptr, 16);
    assert(t < 256);
    data.push_back(static_cast<uint8_t>(t));
  }
  return data;
}

inline std::string bytes2str(bytes const &data) {
  // convert it to str
  std::stringstream ss;
  ss << std::hex << std::noshowbase << std::setfill('0');
  for (auto const &d : data) {
    ss << std::setfill('0') << std::setw(2) << unsigned(d);
  }
  return ss.str();
}

}  // namespace taraxa
