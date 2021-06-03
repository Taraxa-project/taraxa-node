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

template <typename... Params>
void enc_rlp_tuple(RLPStream& rlp, Params const&... args);

template <typename Param>
auto enc_rlp(RLPStream& rlp, Param const& target) -> decltype(rlp.append(target), void()) {
  rlp.append(target);
}

template <typename T>
struct RLPNull {};

template <typename Param>
void enc_rlp(RLPStream& rlp, RLPNull<Param>) {
  rlp.append(unsigned(0));
}

template <typename Param>
void enc_rlp(RLPStream& rlp, optional<Param> const& target) {
  if (target) {
    enc_rlp(rlp, *target);
  } else {
    static const RLPNull<Param> null;
    enc_rlp(rlp, null);
  }
}

template <typename Param>
void enc_rlp(RLPStream& rlp, Param* target) {
  rlp.append(bigint(reinterpret_cast<uintptr_t>(target)));
}

template <typename Param>
void enc_rlp(RLPStream& rlp, RangeView<Param> const& target) {
  rlp.appendList(target.size);
  target.for_each([&](auto const& el) { enc_rlp(rlp, el); });
}

template <typename Map>
auto enc_rlp(RLPStream& rlp, Map const& target)
    -> decltype(target.size(), target.begin()->first, target.end()->second, void()) {
  rlp.appendList(target.size());
  for (auto const& [k, v] : target) {
    enc_rlp_tuple(rlp, k, v);
  }
}

template <typename Param, typename... Params>
void enc_rlp(RLPStream& rlp, Param const& target, Params const&... rest) {
  enc_rlp(rlp, target);
  enc_rlp(rlp, rest...);
}

template <typename... Params>
void enc_rlp_tuple(RLPStream& rlp, Params const&... args) {
  rlp.appendList(sizeof...(args));
  enc_rlp(rlp, args...);
}

template <typename... Params>
void dec_rlp_tuple(RLP const& rlp, Params&... args);

template <typename Param>
auto dec_rlp(RLP const& rlp, Param& target) -> decltype(Param(rlp), void()) {
  target = Param(rlp);
}

template <typename Param>
void dec_rlp(RLP const& rlp, optional<Param>& target) {
  if (rlp.isNull() || rlp.isEmpty()) {
    target = nullopt;
  } else {
    dec_rlp(rlp, target.emplace());
  }
}

template <typename Sequence>
void dec_rlp_sequence(RLP const& rlp, Sequence& target) {
  for (auto const i : rlp) {
    dec_rlp(i, target.emplace_back());
  }
}

inline void dec_rlp(RLP const& rlp, bool& target) { target = rlp.toInt<uint8_t>(); }

inline void dec_rlp(RLP const& rlp, bytes& target) { target = bytes(rlp); }

template <typename... Ts>
void dec_rlp(RLP const& rlp, vector<Ts...>& target) {
  dec_rlp_sequence(rlp, target);
}

template <typename Map>
auto dec_rlp(RLP const& rlp, Map& target) -> decltype(target[target.begin()->first], void()) {
  using key_type = remove_cv_t<decltype(target.begin()->first)>;
  for (auto const i : rlp) {
    auto entry_i = i.begin();
    key_type key;
    dec_rlp(*entry_i, key);
    dec_rlp(*(++entry_i), target[key]);
  }
}

template <typename Param, typename... Params>
void dec_rlp_tuple_body(RLP::iterator& rlp, Param& target, Params&... rest) {
  dec_rlp(*rlp, target);
  if constexpr (sizeof...(rest) > 0) {
    dec_rlp_tuple_body(++rlp, rest...);
  }
}

template <typename... Params>
void dec_rlp_tuple(RLP const& rlp, Params&... args) {
  auto rlp_list = rlp.begin();
  dec_rlp_tuple_body(rlp_list, args...);
}

}  // namespace taraxa::util::encoding_rlp

namespace taraxa::util {
using encoding_rlp::dec_rlp;
using encoding_rlp::dec_rlp_sequence;
using encoding_rlp::dec_rlp_tuple;
using encoding_rlp::enc_rlp;
using encoding_rlp::enc_rlp_tuple;
using encoding_rlp::RLPNull;
}  // namespace taraxa::util
