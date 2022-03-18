#pragma once

#include <libdevcore/RLP.h>

#include <optional>
#include <unordered_map>
#include <vector>

#include "common/range_view.hpp"
#include "common/util.hpp"

namespace taraxa::util::encoding_rlp {
using dev::RLP;

using RLPEncoderRef = dev::RLPStream&;
struct RLPDecoderRef {
  RLP const& value;
  RLP::Strictness strictness;

  RLPDecoderRef(RLP const& value, RLP::Strictness strictness) : value(value), strictness(strictness) {}
  RLPDecoderRef(RLP const& value, bool strict = false)
      : RLPDecoderRef(value, strict ? RLP::VeryStrict : RLP::LaissezFaire) {}
};

template <typename... Params>
void rlp_tuple(RLPEncoderRef encoding, Params const&... args);

template <typename T>
auto rlp(RLPEncoderRef encoding, T const& target) -> decltype(RLP().toInt<T>(), void()) {
  encoding.append(target);
}

template <unsigned N>
void rlp(RLPEncoderRef encoding, dev::FixedHash<N> const& target) {
  encoding.append(target);
}

template <typename T>
inline auto rlp(RLPEncoderRef encoding, T const& target) -> decltype(target.rlp(encoding), void()) {
  target.rlp(encoding);
}

inline auto rlp(RLPEncoderRef encoding, std::string const& target) { encoding.append(target); }

inline auto rlp(RLPEncoderRef encoding, bytes const& target) { encoding.append(target); }

template <typename Param>
void rlp(RLPEncoderRef encoding, std::optional<Param> const& target) {
  if (target) {
    rlp(encoding, *target);
  } else {
    encoding.append(unsigned(0));
  }
}

template <typename Param>
void rlp(RLPEncoderRef encoding, RangeView<Param> const& target) {
  encoding.appendList(target.size);
  target.for_each([&](auto const& el) { rlp(encoding, el); });
}

template <typename T1, typename T2>
void rlp(RLPEncoderRef encoding, std::pair<T1, T2> const& target) {
  rlp_tuple(encoding, target.first, target.second);
}

template <typename Sequence>
auto rlp(RLPEncoderRef encoding, Sequence const& target)
    -> decltype(target.size(), target.begin(), target.end(), void()) {
  encoding.appendList(target.size());
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
  encoding.appendList(num_elements);
  __enc_rlp_tuple_body__(encoding, args...);
}

template <typename... Params>
void rlp_tuple(RLPDecoderRef encoding, Params&... args);

template <typename T>
auto rlp(RLPDecoderRef encoding, T& target) -> decltype(encoding.value.toInt<T>(), void()) {
  target = encoding.value.toInt<T>(encoding.strictness);
}

template <unsigned N>
void rlp(RLPDecoderRef encoding, dev::FixedHash<N>& target) {
  target = encoding.value.toHash<dev::FixedHash<N>>(encoding.strictness);
}

inline auto rlp(RLPDecoderRef encoding, std::string& target) { target = encoding.value.toString(encoding.strictness); }

inline auto rlp(RLPDecoderRef encoding, bytes& target) { target = encoding.value.toBytes(encoding.strictness); }

template <typename Param>
void rlp(RLPDecoderRef encoding, std::optional<Param>& target) {
  if (encoding.value.isNull() || encoding.value.isEmpty()) {
    target = std::nullopt;
  } else {
    rlp(encoding, target.emplace());
  }
}

template <typename Sequence>
auto rlp(RLPDecoderRef encoding, Sequence& target) -> decltype(target.emplace_back(), void()) {
  for (auto i : encoding.value) {
    rlp(RLPDecoderRef(i, encoding.strictness), target.emplace_back());
  }
}
inline void rlp(RLPDecoderRef encoding, bool& target) { target = encoding.value.toInt<uint8_t>(encoding.strictness); }

template <typename T>
inline auto rlp(RLPDecoderRef encoding, T& target) -> decltype(target.rlp(encoding), void()) {
  target.rlp(encoding);
}

template <typename Map>
auto rlp(RLPDecoderRef encoding, Map& target) -> decltype(target[target.begin()->first], void()) {
  using key_type = std::remove_cv_t<decltype(target.begin()->first)>;
  for (auto i : encoding.value) {
    auto entry_i = i.begin();
    key_type key;
    rlp(RLPDecoderRef(*entry_i, encoding.strictness), key);
    rlp(RLPDecoderRef(*(++entry_i), encoding.strictness), target[key]);
  }
}

template <typename Param, typename... Params>
void __dec_rlp_tuple_body__(RLP::iterator& i, RLP::iterator const& end, RLP::Strictness strictness, Param& target,
                            Params&... rest) {
  if (i == end) {
    return;
  }

  rlp(RLPDecoderRef(*i, strictness), target);

  if constexpr (sizeof...(rest) > 0) {
    __dec_rlp_tuple_body__(++i, end, strictness, rest...);
  }
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

  if (encoding.value.itemCount() != num_elements) {
    throw InvalidEncodingSize(num_elements, encoding.value.itemCount());
  }

  auto it_begin = encoding.value.begin();
  __dec_rlp_tuple_body__(it_begin, encoding.value.end(), encoding.strictness, args...);
}

template <typename T>
T rlp_dec(RLPDecoderRef encoding) {
  T ret;
  rlp(encoding, ret);
  return ret;
}

template <typename T>
bytes const& rlp_enc(RLPEncoderRef encoder_to_reuse, T const& obj) {
  encoder_to_reuse.clear();
  rlp(encoder_to_reuse, obj);
  return encoder_to_reuse.out();
}

template <typename T>
bytes rlp_enc(T const& obj) {
  dev::RLPStream s;
  rlp(s, obj);
  return move(s.invalidate());
}

}  // namespace taraxa::util::encoding_rlp

#define HAS_RLP_FIELDS                                            \
  void rlp(::taraxa::util::encoding_rlp::RLPDecoderRef encoding); \
  void rlp(::taraxa::util::encoding_rlp::RLPEncoderRef encoding) const;

#define RLP_FIELDS_DEFINE(_class_, ...)                                           \
  void _class_::rlp(::taraxa::util::encoding_rlp::RLPDecoderRef encoding) {       \
    ::taraxa::util::encoding_rlp::rlp_tuple(encoding, __VA_ARGS__);               \
  }                                                                               \
  void _class_::rlp(::taraxa::util::encoding_rlp::RLPEncoderRef encoding) const { \
    ::taraxa::util::encoding_rlp::rlp_tuple(encoding, __VA_ARGS__);               \
  }

#define RLP_FIELDS_DEFINE_INPLACE(...)                                   \
  void rlp(::taraxa::util::encoding_rlp::RLPDecoderRef encoding) {       \
    ::taraxa::util::encoding_rlp::rlp_tuple(encoding, __VA_ARGS__);      \
  }                                                                      \
  void rlp(::taraxa::util::encoding_rlp::RLPEncoderRef encoding) const { \
    ::taraxa::util::encoding_rlp::rlp_tuple(encoding, __VA_ARGS__);      \
  }

namespace taraxa::util {
using encoding_rlp::InvalidEncodingSize;
using encoding_rlp::rlp;
using encoding_rlp::rlp_dec;
using encoding_rlp::rlp_enc;
using encoding_rlp::rlp_tuple;
using encoding_rlp::RLPDecoderRef;
using encoding_rlp::RLPEncoderRef;
}  // namespace taraxa::util
