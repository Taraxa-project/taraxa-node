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
/** @file BlockHeader.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "BlockHeader.h"

#include <libdevcore/Common.h>
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>

#include "Common.h"
#include "Exceptions.h"
#include "TrieHash.h"

using namespace std;
using namespace dev;
using namespace ::taraxa::aleth;

void BlockHeaderFields::streamRLP(RLPStream& _s) const {
  _s.appendList(15);
  _s << m_parentHash << m_sha3Uncles << m_author << m_stateRoot << m_transactionsRoot << m_receiptsRoot << m_logBloom
     << m_difficulty << m_number << m_gasLimit << m_gasUsed << m_timestamp << m_extraData << m_mixHash << m_nonce;
}

BlockHeader::BlockHeader(RLP const& rlp) : m_hash(sha3(rlp.data())) {
  int field = 0;
  try {
    m_parentHash = rlp[field = 0].toHash<h256>(RLP::VeryStrict);
    m_sha3Uncles = rlp[field = 1].toHash<h256>(RLP::VeryStrict);
    m_author = rlp[field = 2].toHash<Address>(RLP::VeryStrict);
    m_stateRoot = rlp[field = 3].toHash<h256>(RLP::VeryStrict);
    m_transactionsRoot = rlp[field = 4].toHash<h256>(RLP::VeryStrict);
    m_receiptsRoot = rlp[field = 5].toHash<h256>(RLP::VeryStrict);
    m_logBloom = rlp[field = 6].toHash<LogBloom>(RLP::VeryStrict);
    m_difficulty = rlp[field = 7].toInt<u256>();
    m_number = rlp[field = 8].toInt<BlockNumber>();
    m_gasLimit = rlp[field = 9].toInt<uint64_t>();
    m_gasUsed = rlp[field = 10].toInt<uint64_t>();
    m_timestamp = rlp[field = 11].toInt<uint64_t>();
    m_extraData = rlp[field = 12].toBytes();
    m_mixHash = rlp[field = 13].toHash<h256>();
    m_nonce = rlp[field = 14].toHash<h64>();
  } catch (Exception const& _e) {
    _e << errinfo_name("invalid block header format") << BadFieldError(field, toHex(rlp[field].data().toBytes()));
    throw;
  }
}

h256 const& BlockHeader::hash() const {
  if (!m_hash) {
    RLPStream s;
    streamRLP(s);
    m_hash = sha3(s.out());
  }
  return m_hash;
}
