#include "final_chain/trie_common.hpp"

#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>

namespace taraxa::final_chain {
using namespace ::dev;

/*
 * Hex-prefix Notation. First nibble has flags: oddness = 2^0 & termination =
 * 2^1 NOTE: the "termination marker" and "leaf-node" specifier are completely
 * equivalent. [0,0,1,2,3,4,5]   0x10012345 [0,1,2,3,4,5]     0x00012345
 * [1,2,3,4,5]       0x112345
 * [0,0,1,2,3,4]     0x00001234
 * [0,1,2,3,4]       0x101234
 * [1,2,3,4]         0x001234
 * [0,0,1,2,3,4,5,T] 0x30012345
 * [0,0,1,2,3,4,T]   0x20001234
 * [0,1,2,3,4,5,T]   0x20012345
 * [1,2,3,4,5,T]     0x312345
 * [1,2,3,4,T]       0x201234
 */
std::string hexPrefixEncode(bytes const& _hexVector, bool _leaf, int _begin = 0, int _end = -1) {
  unsigned begin = _begin;
  unsigned end = _end < 0 ? _hexVector.size() + 1 + _end : _end;
  bool odd = ((end - begin) % 2) != 0;

  std::string ret(1, ((_leaf ? 2 : 0) | (odd ? 1 : 0)) * 16);
  if (odd) {
    ret[0] |= _hexVector[begin];
    ++begin;
  }
  for (unsigned i = begin; i < end; i += 2) {
    ret += _hexVector[i] * 16 + _hexVector[i + 1];
  }
  return ret;
}

void hash256aux(HexMap const& _s, HexMap::const_iterator _begin, HexMap::const_iterator _end, unsigned _preLen,
                RLPStream& _rlp);

void hash256rlp(HexMap const& _s, HexMap::const_iterator _begin, HexMap::const_iterator _end, unsigned _preLen,
                RLPStream& _rlp) {
  if (_begin == _end) {
    _rlp << "";  // NULL
  } else if (std::next(_begin) == _end) {
    // only one left - terminate with the pair.
    _rlp.appendList(2) << hexPrefixEncode(_begin->first, true, _preLen) << _begin->second;
  } else {
    // find the number of common prefix nibbles shared
    // i.e. the minimum number of nibbles shared at the beginning between the
    // first hex string and each successive.
    auto sharedPre = (unsigned)-1;
    unsigned c = 0;
    for (auto i = std::next(_begin); i != _end && sharedPre; ++i, ++c) {
      unsigned x = std::min(sharedPre, std::min((unsigned)_begin->first.size(), (unsigned)i->first.size()));
      unsigned shared = _preLen;
      for (; shared < x && _begin->first[shared] == i->first[shared]; ++shared)
        ;
      sharedPre = std::min(shared, sharedPre);
    }
    if (sharedPre > _preLen) {
      // if they all have the same next nibble, we also want a pair.
      _rlp.appendList(2) << hexPrefixEncode(_begin->first, false, _preLen, (int)sharedPre);
      hash256aux(_s, _begin, _end, (unsigned)sharedPre, _rlp);
    } else {
      // otherwise enumerate all 16+1 entries.
      _rlp.appendList(17);
      auto b = _begin;
      if (_preLen == b->first.size()) {
        ++b;
      }
      for (auto i = 0; i < 16; ++i) {
        auto n = b;
        for (; n != _end && n->first[_preLen] == i; ++n)
          ;
        if (b == n) {
          _rlp << "";
        } else {
          hash256aux(_s, b, n, _preLen + 1, _rlp);
        }
        b = n;
      }
      if (_preLen == _begin->first.size()) {
        _rlp << _begin->second;
      } else {
        _rlp << "";
      }
    }
  }
}

void hash256aux(HexMap const& _s, HexMap::const_iterator _begin, HexMap::const_iterator _end, unsigned _preLen,
                RLPStream& _rlp) {
  RLPStream rlp;
  hash256rlp(_s, _begin, _end, _preLen, rlp);
  if (rlp.out().size() < 32) {
    // RECURSIVE RLP
    _rlp.appendRaw(rlp.out());
  } else {
    _rlp << sha3(rlp.out());
  }
}

h256 hash256(BytesMap const& _s) {
  if (_s.empty()) {
    static auto const empty_trie = sha3(RLPStream().append("").out());
    return empty_trie;
  }
  // build patricia tree.
  HexMap hexMap;
  for (auto i = _s.rbegin(); i != _s.rend(); ++i) hexMap[asNibbles(bytesConstRef(&i->first))] = i->second;
  RLPStream s;
  hash256rlp(hexMap, hexMap.cbegin(), hexMap.cend(), 0, s);
  return sha3(s.out());
}

}  // namespace taraxa::final_chain
