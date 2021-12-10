#pragma once

#include <limits>

// #include "common/event.hpp"
// #include "common/range_view.hpp"
#include "common/types.hpp"
// #include "config/final_chain_config.hpp"
#include "final_chain/final_chain.hpp"

namespace taraxa::final_chain {
class ContractInterface {
 public:
  ContractInterface(addr_t contract_addr, std::shared_ptr<FinalChain> final_chain);
  ~ContractInterface() = default;

  template <typename Result, typename... Params>
  Result call(const string& function, const Params&... args);

  template <typename Result>
  static Result unpack(bytes const& data);

  template <typename T, std::enable_if_t<std::numeric_limits<T>::is_integer>* = nullptr>
  static auto unpack(bytes& data, T& num) {
    num = fromBigEndian<T>(data);
  }

  /// PACKING ///
  template <typename... Params>
  static bytes pack(const string& function, const Params&... args) {
    bytes data = getFunction(function);
    pack(data, args...);
    return data;
  }

  template <typename Param, typename... Params>
  static void pack(bytes& data, const Param& arg, const Params&... args) {
    pack(data, arg);
    pack(data, args...);
  }

  template <typename T, std::enable_if_t<std::numeric_limits<T>::is_integer>* = nullptr>
  static void pack(bytes& data, T num) {
    boost::multiprecision::uint256_t big_num{num};
    auto tmp = dev::toBigEndian(big_num);
    data.insert(data.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
  }

  static void pack(bytes& data, const addr_t& addr) {
    data.insert(data.end(), 12, 0);
    data.insert(data.end(), addr.begin(), addr.end());
  }

  static void pack(bytes& data, bool flag) {
    boost::multiprecision::uint256_t big_num{flag};
    auto tmp = dev::toBigEndian(big_num);
    data.insert(data.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
  }

  static void pack(bytes& data, const bytes& value) {
    assert(value.size() <= 32);
    data.insert(data.end(), value.begin(), value.end());
    data.insert(data.end(), 32 - value.size(), 0);
  }

  template <typename T>
  static void pack(bytes& data, const std::vector<T>& array) {
    pack(data, array.size());
    for (const auto& value : array) {
      pack(data, value);
    }
  }

  template <typename T, std::size_t N>
  static void pack(bytes& data, const std::array<T, N>& array) {
    for (const auto& value : array) {
      pack(data, value);
    }
  }
  ////////////

  static bytes getFunction(const string& function);

 private:
  bytes finalChainCall(bytes data);

  const addr_t contract_addr_;
  std::shared_ptr<FinalChain> final_chain_;
};

}  // namespace taraxa::final_chain