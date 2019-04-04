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
#include "SecureTrieDB.h"
#include "ValidationSchemes.h"
#include <libdevcore/JsonUtils.h>
#include <libdevcore/OverlayDB.h>
#include <libethcore/ChainOperationParams.h>
#include <libethcore/Precompiled.h>

using namespace std;
using namespace dev;
using namespace dev::eth;

namespace fs = boost::filesystem;

void Account::setCode(bytes&& _code)
{
    m_codeCache = std::move(_code);
    m_hasNewCode = true;
    m_codeHash = sha3(m_codeCache);
}

u256 Account::originalStorageValue(u256 const& _key, OverlayDB const& _db) const
{
    auto it = m_storageOriginal.find(_key);
    if (it != m_storageOriginal.end())
        return it->second;

    // Not in the original values cache - go to the DB.
    SecureTrieDB<h256, OverlayDB> const memdb(const_cast<OverlayDB*>(&_db), m_storageRoot);
    std::string const payload = memdb.at(_key);
    auto const value = payload.size() ? RLP(payload).toInt<u256>() : 0;
    m_storageOriginal[_key] = value;
    return value;
}

namespace js = json_spirit;

namespace
{

uint64_t toUnsigned(js::mValue const& _v)
{
    switch (_v.type())
    {
    case js::int_type: return _v.get_uint64();
    case js::str_type: return fromBigEndian<uint64_t>(fromHex(_v.get_str()));
    default: return 0;
    }
}

PrecompiledContract createPrecompiledContract(js::mObject const& _precompiled)
{
    auto n = _precompiled.at("name").get_str();
    try
    {
        u256 startingBlock = 0;
        if (_precompiled.count("startingBlock"))
            startingBlock = u256(_precompiled.at("startingBlock").get_str());

        if (!_precompiled.count("linear"))
            return PrecompiledContract(PrecompiledRegistrar::pricer(n), PrecompiledRegistrar::executor(n), startingBlock);

        auto const& l = _precompiled.at("linear").get_obj();
        unsigned base = toUnsigned(l.at("base"));
        unsigned word = toUnsigned(l.at("word"));
        return PrecompiledContract(base, word, PrecompiledRegistrar::executor(n), startingBlock);
    }
    catch (PricerNotFound const&)
    {
        cwarn << "Couldn't create a precompiled contract account. Missing a pricer called:" << n;
        throw;
    }
    catch (ExecutorNotFound const&)
    {
        // Oh dear - missing a plugin?
        cwarn << "Couldn't create a precompiled contract account. Missing an executor called:" << n;
        throw;
    }
}
}
