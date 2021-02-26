#pragma once

#include "common/types.hpp"

namespace taraxa::final_chain {

h256 hash256(dev::BytesMap const& _s);

template <class T, class U>
inline auto trieRootOver(unsigned _itemCount, T const& _getKey, U const& _getValue) {
  dev::BytesMap m;
  for (unsigned i = 0; i < _itemCount; ++i) {
    m[_getKey(i)] = _getValue(i);
  }
  return hash256(m);
}

}  // namespace taraxa::final_chain
