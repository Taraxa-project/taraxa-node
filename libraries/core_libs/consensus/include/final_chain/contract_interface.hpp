#pragma once

#include <libdevcore/Common.h>

#include <limits>

#include "common/types.hpp"
#include "final_chain/final_chain.hpp"
#include "final_chain/rewards_stats.hpp"

namespace taraxa::final_chain {
class ContractInterface {
 public:
  /// PACKING ///
  template <typename... Params>
  static bytes packFunctionCall(const string& function, const Params&... args) {
    auto output = getFunction(function);
    auto args_data = pack(args...);
    output.insert(output.end(), std::make_move_iterator(args_data.begin()), std::make_move_iterator(args_data.end()));
    return output;
  }

  template <typename Param, typename... Params>
  static bytes pack(const Param& arg, const Params&... args) {
    auto output = pack(arg);
    auto data = pack(args...);
    output.insert(output.end(), std::make_move_iterator(data.begin()), std::make_move_iterator(data.end()));
    return output;
  }

  template <typename T, std::enable_if_t<std::numeric_limits<T>::is_integer>* = nullptr>
  static bytes pack(T num) {
    bytes data;
    boost::multiprecision::uint256_t big_num{num};
    auto tmp = dev::toBigEndian(big_num);
    data.insert(data.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
    return data;
  }

  static bytes pack(const addr_t& addr) {
    bytes data;
    data.insert(data.end(), 12, 0);
    data.insert(data.end(), addr.begin(), addr.end());
    return data;
  }

  static bytes pack(bool flag) {
    bytes data;
    boost::multiprecision::uint256_t big_num{flag};
    auto tmp = dev::toBigEndian(big_num);
    data.insert(data.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
    return data;
  }

  static bytes pack(const bytes& value) {
    bytes data;
    // encode length of msg
    if (value.size()) {
      data = pack(value.size());
    }
    // encode msg
    data.insert(data.end(), value.begin(), value.end());
    // trailing zeros
    data.insert(data.end(), 32 - (value.size() % 32), 0);
    return data;
  }
  ////////////

  static bytes getFunction(const string& function) { return dev::sha3(function.c_str()).ref().cropped(0, 4).toBytes(); }
};

}  // namespace taraxa::final_chain