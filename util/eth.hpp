#ifndef TARAXA_NODE_UTIL_ETH_HPP_
#define TARAXA_NODE_UTIL_ETH_HPP_

#include <libdevcore/Assertions.h>
#include <libdevcore/DBFactory.h>
#include <libethcore/Exceptions.h>
#include <libethereum/Account.h>
#include <libethereum/SecureTrieDB.h>
#include <boost/filesystem.hpp>
#include <tuple>
#include <type_traits>

namespace taraxa::util::eth {
using namespace std;
using namespace dev;
using namespace dev::db;
using namespace dev::eth;
namespace fs = boost::filesystem;

inline Slice toSlice(h256 const& _h) {
  return Slice(reinterpret_cast<char const*>(_h.data()), _h.size);
}

inline Slice toSlice(string const& _str) {
  return Slice(_str.data(), _str.size());
}

inline Slice toSlice(bytes const& _b) {
  return Slice(reinterpret_cast<char const*>(&_b[0]), _b.size());
}

template <class N, typename = std::enable_if_t<std::is_arithmetic<N>::value>>
inline Slice toSlice(N const& n) {
  return Slice(reinterpret_cast<char const*>(&n), sizeof(N));
}

// partially copied from /libethereum/State.cpp
inline bool isDiskDatabase(const DatabaseKind& kind) {
  switch (kind) {
    case DatabaseKind::LevelDB:
    case DatabaseKind::RocksDB:
      return true;
    default:
      return false;
  }
}

template <typename DB>
inline h256 calculateGenesisState(DB& db, AccountMap const& accounts) {
  SecureTrieDB<Address, DB> state(&db);
  state.init();
  commit(accounts, state);
  return state.root();
}

// partially copied from /libethereum/State.cpp
struct DBAndMeta {
  unique_ptr<DatabaseFace> db;
  DatabaseKind kind;
  fs::path path;
};
inline DBAndMeta newDB(fs::path const& _basePath, h256 const& _genesisHash,
                       WithExisting _we, DatabaseKind kind = databaseKind()) {
  auto isDiskDB = isDiskDatabase(kind);
  auto path = _basePath.empty() ? db::databasePath() : _basePath;
  path /= fs::path(toHex(_genesisHash.ref().cropped(0, 4)));
  if (isDiskDB && _we == WithExisting::Kill) {
    fs::remove_all(path);
  }
  if (isDiskDB) {
    fs::create_directories(path);
    DEV_IGNORE_EXCEPTIONS(fs::permissions(path, fs::owner_all));
  }
  try {
    auto db = DBFactory::create(kind, path);
    return {move(db), kind, path};
  } catch (boost::exception const& ex) {
    cwarn << boost::diagnostic_information(ex) << '\n';
    if (!isDiskDB)
      throw;
    else if (fs::space(path).available < 1024) {
      cwarn << "Not enough available space found on hard drive. Please free "
               "some up and then re-run. Bailing.";
      BOOST_THROW_EXCEPTION(NotEnoughAvailableSpace());
    } else {
      cwarn << "Database " << (path) << "already open. Bailing.";
      BOOST_THROW_EXCEPTION(DatabaseAlreadyOpen());
    }
  }
}

}  // namespace taraxa::util::eth

#endif  // TARAXA_NODE_UTIL_ETH_HPP_
