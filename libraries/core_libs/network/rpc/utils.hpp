#ifndef TARAXA_NET_RPC_UTILS_HPP
#define TARAXA_NET_RPC_UTILS_HPP

#include <libdevcore/CommonJS.h>

#include <optional>
#include <string>

#include "common/types.hpp"

namespace taraxa::net {
// Please read README
const int JSON_ANY = 0;

static std::optional<taraxa::EthBlockNumber> parse_blk_num_specific(const std::string& blk_num_str) {
  if (blk_num_str == "latest" || blk_num_str == "pending" || blk_num_str == "safe" || blk_num_str == "finalized") {
    return std::nullopt;
  }
  return blk_num_str == "earliest" ? 0 : dev::jsToInt(blk_num_str);
}

}  // namespace taraxa::net

#endif  // TARAXA_NET_RPC_UTILS_HPP