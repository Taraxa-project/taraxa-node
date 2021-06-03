#pragma once

#include <libdevcore/RLP.h>

#include <optional>
#include <unordered_map>
#include <vector>

#include "range_view.hpp"
#include "util.hpp"

namespace taraxa::util::encoding_rlp {
using namespace std;
using namespace dev;

struct RLPEncoderRef {
  RLPStream& target;

  RLPEncoderRef(RLPStream& target) : target(target) {}
};

struct RLPDecoderRef {
  RLP const& target;
  RLP::Strictness strictness;

  RLPDecoderRef(RLP const& target, RLP::Strictness strictness) : target(target), strictness(strictness) {}
  RLPDecoderRef(RLP const& target, bool strict = false)
      : RLPDecoderRef(target, strict ? RLP::VeryStrict : RLP::LaissezFaire) {}
};

template <typename... Params>
void rlp_tuple(RLPEncoderRef encoding, Params const&... args);

template <typename T>
auto rlp(RLPEncoderRef encoding, T const& target) -> decltype(RLP().toInt<T>(), void()) {
  encoding.target.append(target);
}

template <unsigned N>
void rlp(RLPEncoderRef encoding, FixedHash<N> const& target) {
  encoding.target.append(target);
}

template <typename T>
inline auto rlp(RLPEncoderRef encoding, T const& target) -> decltype(target.rlp(encoding), void()) {
  target.rlp(encoding);
}

inline auto rlp(RLPEncoderRef encoding, string const& target) { encoding.target.append(target); }

inline auto rlp(RLPEncoderRef encoding, bytes const& target) { encoding.target.append(target); }

template <typename Param>
void rlp(RLPEncoderRef encoding, optional<Param> const& target) {
  if (target) {
    rlp(encoding, *target);
  } else {
    encoding.target.append(unsigned(0));
  }
}

template <typename Param>
void rlp(RLPEncoderRef encoding, RangeView<Param> const& target) {
  encoding.target.appendList(target.size);
  target.for_each([&](auto const& el) { rlp(encoding, el); });
}

template <typename T1, typename T2>
void rlp(RLPEncoderRef encoding, std::pair<T1, T2> const& target) {
  rlp_tuple(encoding, target.first, target.second);
}

template <typename Sequence>
auto rlp(RLPEncoderRef encoding, Sequence const& target)
    -> decltype(target.size(), target.begin(), target.end(), void()) {
  encoding.target.appendList(target.size());
  for (auto const& v : target) {
    rlp(encoding, v);
  }
}

template <typename Param, typename... Params>
void __enc_rlp_tuple_body__(RLPEncoderRef encoding, Param const& target, Params const&... rest) {
  rlp(encoding, target);
  if constexpr (sizeof...(rest) != 0) {
    __enc_rlp_tuple_body__(encoding, rest...);
  }
}

template <typename... Params>
void rlp_tuple(RLPEncoderRef encoding, Params const&... args) {
  constexpr auto num_elements = sizeof...(args);
  static_assert(0 < num_elements);
  encoding.target.appendList(num_elements);
  __enc_rlp_tuple_body__(encoding, args...);
}

template <typename... Params>
void rlp_tuple(RLPDecoderRef encoding, Params&... args);

template <typename T>
auto rlp(RLPDecoderRef encoding, T& target) -> decltype(encoding.target.toInt<T>(), void()) {
  target = encoding.target.toInt<T>(encoding.strictness);
}

template <unsigned N>
void rlp(RLPDecoderRef encoding, FixedHash<N>& target) {
  target = encoding.target.toHash<FixedHash<N>>(encoding.strictness);
}

inline auto rlp(RLPDecoderRef encoding, string& target) { target = encoding.target.toString(encoding.strictness); }

inline auto rlp(RLPDecoderRef encoding, bytes& target) { target = encoding.target.toBytes(encoding.strictness); }

template <typename Param>
void rlp(RLPDecoderRef encoding, optional<Param>& target) {
  if (encoding.target.isNull() || encoding.target.isEmpty()) {
    target = nullopt;
  } else {
    rlp(encoding, target.emplace());
  }
}

template <typename Sequence>
auto rlp(RLPDecoderRef encoding, Sequence& target) -> decltype(target.emplace_back(), void()) {
  for (auto const& i : encoding.target) {
    rlp(RLPDecoderRef(i, encoding.strictness), target.emplace_back());
  }
}
inline void rlp(RLPDecoderRef encoding, bool& target) { target = encoding.target.toInt<uint8_t>(encoding.strictness); }

template <typename T>
inline auto rlp(RLPDecoderRef encoding, T& target) -> decltype(target.rlp(encoding), void()) {
  target.rlp(encoding);
}

template <typename Map>
auto rlp(RLPDecoderRef encoding, Map& target) -> decltype(target[target.begin()->first], void()) {
  using key_type = remove_cv_t<decltype(target.begin()->first)>;
  for (auto const& i : encoding.target) {
    auto entry_i = i.begin();
    key_type key;
    rlp(RLPDecoderRef(*entry_i, encoding.strictness), key);
    rlp(RLPDecoderRef(*(++entry_i), encoding.strictness), target[key]);
  }
}

template <typename Param, typename... Params>
uint __dec_rlp_tuple_body__(RLP::iterator& i, RLP::iterator const& end, RLP::Strictness strictness, Param& target,
                            Params&... rest) {
  if (i == end) {
    return 0;
  }
  rlp(RLPDecoderRef(*i, strictness), target);
  if constexpr (sizeof...(rest) != 0) {
    return 1 + __dec_rlp_tuple_body__(++i, end, strictness, rest...);
  }
  return 1;
}

struct InvalidEncodingSize : std::invalid_argument {
  uint expected, actual;

  InvalidEncodingSize(uint expected, uint actual)
      : invalid_argument(fmt("Invalid rlp list size; expected: %s, actual: %s", expected, actual)),
        expected(expected),
        actual(actual) {}
};

template <typename... Params>
void rlp_tuple(RLPDecoderRef encoding, Params&... args) {
  constexpr auto num_elements = sizeof...(args);
  static_assert(0 < num_elements);
  auto list_begin = encoding.target.begin();
  auto num_elements_processed = __dec_rlp_tuple_body__(list_begin, encoding.target.end(), encoding.strictness, args...);
  if (num_elements != num_elements_processed) {
    throw InvalidEncodingSize(num_elements, num_elements_processed);
  }
}

}  // namespace taraxa::util::encoding_rlp

#define RLP_FIELDS(...)                                                  \
  void rlp(::taraxa::util::encoding_rlp::RLPDecoderRef encoding) {       \
    ::taraxa::util::encoding_rlp::rlp_tuple(encoding, __VA_ARGS__);      \
  }                                                                      \
  void rlp(::taraxa::util::encoding_rlp::RLPEncoderRef encoding) const { \
    ::taraxa::util::encoding_rlp::rlp_tuple(encoding, __VA_ARGS__);      \
  }

namespace taraxa::util {
using encoding_rlp::InvalidEncodingSize;
using encoding_rlp::rlp;
using encoding_rlp::rlp_tuple;
using encoding_rlp::RLPDecoderRef;
using encoding_rlp::RLPEncoderRef;
}  // namespace taraxa::util
