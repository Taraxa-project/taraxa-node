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
using PbftPeriod = EthBlockNumber;
using PbftRound = uint32_t;
using PbftStep = uint32_t;

// time related
using uint256_t = boost::multiprecision::uint256_t;

// network related
using uint256_hash_t = dev::FixedHash<32>;
using uint512_hash_t = dev::FixedHash<64>;
using uint520_hash_t = dev::FixedHash<65>;
using uint160_hash_t = dev::FixedHash<20>;

using secret_t = dev::SecureFixedHash<32>;
using public_t = uint512_hash_t;
using addr_t = uint160_hash_t;
using sig_t = uint520_hash_t;
using vote_hash_t = uint256_hash_t;
using blk_hash_t = uint256_hash_t;
using trx_hash_t = uint256_hash_t;

using gas_t = uint64_t;
using level_t = uint64_t;
using val_t = dev::u256;
using root_t = dev::h256;

using vec_blk_t = std::vector<blk_hash_t>;
using vec_trx_t = std::vector<trx_hash_t>;
using byte = uint8_t;
using bytes = std::vector<byte>;
using trx_nonce_t = val_t;
}  // namespace taraxa
