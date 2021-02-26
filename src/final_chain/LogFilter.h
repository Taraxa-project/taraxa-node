#pragma once

#include <libdevcore/Common.h>

#include "common/types.hpp"
#include "types.h"

namespace taraxa::final_chain {

class LogFilter {
 public:
  LogFilter(BlockNumber _earliest, BlockNumber _latest) : m_earliest(_earliest), m_latest(_latest) {}

  void streamRLP(dev::RLPStream& _s) const;
  h256 sha3() const;

  /// hash of earliest block which should be filtered
  BlockNumber earliest() const { return m_earliest; }

  /// hash of latest block which should be filtered
  BlockNumber latest() const { return m_latest; }

  /// Range filter is a filter which doesn't care about addresses or topics
  /// Matches are all entries from earliest to latest
  /// @returns true if addresses and topics are unspecified
  bool isRangeFilter() const;

  /// @returns bloom possibilities for all addresses and topics
  std::vector<LogBloom> bloomPossibilities() const;

  bool matches(LogBloom _bloom) const;

  struct MatchResult {
    bool all_match = false;
    std::vector<uint> match_positions;
  };
  MatchResult matches(TransactionReceipt const& _r) const;

  LogFilter address(Address _a) {
    m_addresses.insert(_a);
    return *this;
  }
  LogFilter topic(unsigned _index, h256 const& _t) {
    if (_index < 4) m_topics[_index].insert(_t);
    return *this;
  }
  LogFilter withEarliest(BlockNumber _e) {
    m_earliest = _e;
    return *this;
  }
  LogFilter withLatest(BlockNumber _e) {
    m_latest = _e;
    return *this;
  }

 private:
  AddressSet m_addresses;
  std::array<h256Hash, 4> m_topics;
  BlockNumber m_earliest = 0;
  BlockNumber m_latest = 0;
};

}  // namespace taraxa::final_chain
