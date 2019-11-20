/**
 * This file is a modified version of cpp-ethereum Account.
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
