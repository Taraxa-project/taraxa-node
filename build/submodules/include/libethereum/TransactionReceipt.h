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
/** @file TransactionReceipt.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <libdevcore/RLP.h>
#include <libethcore/Common.h>
#include <libethcore/LogEntry.h>

#include <array>
#include <boost/variant/variant.hpp>

namespace dev {
namespace eth {

/// Transaction receipt, constructed either from RLP representation or from
/// individual values. Either a state root or a status code is contained.
/// m_hasStatusCode is true when it contains a status code. Empty state root is
/// not included into RLP-encoding.
class TransactionReceipt {
 public:
  TransactionReceipt(bytesConstRef _rlp);
  TransactionReceipt(h256 const& _root, u256 const& _gasUsed,
                     LogEntries const& _log, Address const& _contractAddress);
  TransactionReceipt(uint8_t _status, u256 const& _gasUsed,
                     LogEntries const& _log, Address const& _contractAddress);

  /// @returns true if the receipt has a status code.  Otherwise the receipt has
  /// a state root (pre-EIP658).
  bool hasStatusCode() const;
  /// @returns the state root.
  /// @throw TransactionReceiptVersionError when the receipt has a status code
  /// instead of a state root.
  h256 const& stateRoot() const;
  /// @returns the status code.
  /// @throw TransactionReceiptVersionError when the receipt has a state root
  /// instead of a status code.
  uint8_t statusCode() const;
  auto const& cumulativeGasUsed() const { return m_gasUsed; }
  LogBloom const& bloom() const { return m_bloom; }
  LogEntries const& log() const { return m_log; }
  Address const& contractAddress() const { return m_contractAddress; }

  void streamRLP(RLPStream& _s) const;

  bytes rlp() const {
    RLPStream s;
    streamRLP(s);
    return s.out();
  }

 private:
  boost::variant<uint8_t, h256> m_statusCodeOrStateRoot;
  uint64_t m_gasUsed = 0;
  LogBloom m_bloom;
  LogEntries m_log;
  Address m_contractAddress;
};

using TransactionReceipts = std::vector<TransactionReceipt>;

std::ostream& operator<<(std::ostream& _out, eth::TransactionReceipt const& _r);

class LocalisedTransactionReceipt : public TransactionReceipt {
 public:
  LocalisedTransactionReceipt(TransactionReceipt const& _t, h256 const& _hash,
                              h256 const& _blockHash, BlockNumber _blockNumber,
                              Address const& _from, Address const& _to,
                              unsigned _transactionIndex, u256 const& _gasUsed)
      : TransactionReceipt(_t),
        m_hash(_hash),
        m_blockHash(_blockHash),
        m_blockNumber(_blockNumber),
        m_from(_from),
        m_to(_to),
        m_transactionIndex(_transactionIndex),
        m_gasUsed(_gasUsed) {
    LogEntries entries = log();
    for (unsigned i = 0; i < entries.size(); i++)
      m_localisedLogs.push_back(LocalisedLogEntry(entries[i], m_blockHash,
                                                  m_blockNumber, m_hash,
                                                  m_transactionIndex, i));
  }

  h256 const& hash() const { return m_hash; }
  h256 const& blockHash() const { return m_blockHash; }
  BlockNumber blockNumber() const { return m_blockNumber; }
  Address const& from() const { return m_from; }
  Address const& to() const { return m_to; }
  unsigned transactionIndex() const { return m_transactionIndex; }
  u256 const& gasUsed() const { return m_gasUsed; }
  LocalisedLogEntries const& localisedLogs() const { return m_localisedLogs; };

 private:
  h256 m_hash;
  h256 m_blockHash;
  BlockNumber m_blockNumber = 0;
  Address m_from;
  Address m_to;
  unsigned m_transactionIndex = 0;
  u256 m_gasUsed;
  LocalisedLogEntries m_localisedLogs;
};

}  // namespace eth
}  // namespace dev
