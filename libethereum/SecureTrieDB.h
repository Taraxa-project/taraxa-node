/*
This file is a modified version of cpp-ethereum Account.
*/

#pragma once

#include <libdevcore/TrieDB.h>

namespace dev
{
namespace eth
{
#if ETH_FATDB
template <class KeyType, class DB>
using SecureTrieDB = SpecificTrieDB<FatGenericTrieDB<DB>, KeyType>;
#else
template <class KeyType, class DB>
using SecureTrieDB = SpecificTrieDB<HashedGenericTrieDB<DB>, KeyType>;
#endif

}  // namespace eth 
}  // namespace dev
