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
/** @file Account.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Account.h"
#include <libdevcore/OverlayDB.h>
#include "SecureTrieDB.h"

using namespace std;
using namespace dev;
using namespace dev::eth;

namespace fs = boost::filesystem;

void Account::setCode(bytes&& _code) {
  m_codeCache = std::move(_code);
  m_hasNewCode = true;
  m_codeHash = sha3(m_codeCache);
}

u256 Account::originalStorageValue(u256 const& _key,
                                   OverlayDB const& _db) const {
  auto it = m_storageOriginal.find(_key);
  if (it != m_storageOriginal.end()) return it->second;

  // Not in the original values cache - go to the DB.
  SecureTrieDB<h256, OverlayDB> const memdb(const_cast<OverlayDB*>(&_db),
                                            m_storageRoot);
  std::string const payload = memdb.at(_key);
  auto const value = payload.size() ? RLP(payload).toInt<u256>() : 0;
  m_storageOriginal[_key] = value;
  return value;
}
