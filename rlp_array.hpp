#ifndef TARAXA_NODE_RLP_ARRAY_HPP_
#define TARAXA_NODE_RLP_ARRAY_HPP_

#include <libdevcore/RLP.h>
#include <libdevcore/TrieHash.h>
#include <functional>

namespace taraxa::rlp_array {
using namespace std;
using namespace dev;

class RlpArray {
  BytesMap value;

 public:
  BytesMap const& getValue() { return value; }
  h256 trieRoot() { return hash256(getValue()); }

  void append(function<void(RLPStream&)> const& writer) {
    RLPStream pos_rlp, element_rlp;
    pos_rlp << value.size();
    writer(element_rlp);
    value[pos_rlp.out()] = element_rlp.out();
  }
};

}  // namespace taraxa::rlp_array

namespace taraxa {
using rlp_array::RlpArray;
}

#endif  // TARAXA_NODE__RLP_ARRAY_HPP_
