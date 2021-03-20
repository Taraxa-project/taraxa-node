/*
        This file is part of cpp-ethereum.

        cpp-ethereum is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        cpp-ethereum is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <libdevcore/Address.h>
#include <libdevcore/FixedHash.h>

#include "Common.h"

namespace dev {

class RLP;
class RLPStream;

namespace eth {

struct LogEntry {
  LogEntry() = default;
  explicit LogEntry(RLP const& _r);
  LogEntry(Address const& _address, h256s _topics, bytes _data)
      : address(_address), topics(std::move(_topics)), data(std::move(_data)) {}

  void streamRLP(RLPStream& _s) const;

  LogBloom bloom() const;

  Address address;
  h256s topics;
  bytes data;
};

using LogEntries = std::vector<LogEntry>;

struct LocalisedLogEntry : public LogEntry {
  LocalisedLogEntry() = default;
  LocalisedLogEntry(LogEntry const& _le, h256 const& _blockHash,
                    BlockNumber _blockNumber, h256 const& _transactionHash,
                    unsigned _transactionIndex, unsigned _logIndex)
      : LogEntry(_le),
        blockHash(_blockHash),
        blockNumber(_blockNumber),
        transactionHash(_transactionHash),
        transactionIndex(_transactionIndex),
        logIndex(_logIndex) {}

  h256 blockHash;
  BlockNumber blockNumber = 0;
  h256 transactionHash;
  unsigned transactionIndex = 0;
  unsigned logIndex = 0;
};

using LocalisedLogEntries = std::vector<LocalisedLogEntry>;

inline LogBloom bloom(LogEntries const& _logs) {
  LogBloom ret;
  for (auto const& l : _logs) ret |= l.bloom();
  return ret;
}

}  // namespace eth
}  // namespace dev
