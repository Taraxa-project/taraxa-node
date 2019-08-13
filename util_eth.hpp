#ifndef TARAXA_NODE_UTIL_ETH_HPP
#define TARAXA_NODE_UTIL_ETH_HPP

#include <libdevcore/Assertions.h>
#include <libdevcore/DBFactory.h>
#include <libdevcore/RLP.h>
#include <boost/filesystem.hpp>
#include <functional>

namespace taraxa::util_eth {
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
  path /= fs::path(toHex(_genesisHash.ref().cropped(0, 4))) /
          fs::path(toString(9 + (23 << 9)));  // copied from libethcore/Common.c
  if (isDiskDB && _we == WithExisting::Kill) {
    clog(VerbosityDebug, "statedb")
        << "Killing state database (WithExisting::Kill).";
    fs::remove_all(path / fs::path("state"));
  }
  if (isDiskDB) {
    fs::create_directories(path);
    DEV_IGNORE_EXCEPTIONS(fs::permissions(path, fs::owner_all));
  }
  path = path / fs::path("state");
  try {
    auto db = DBFactory::create(kind, path);
    clog(VerbosityTrace, "statedb") << "Opened state DB.";
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
      cwarn << "Database " << (path)
            << "already open. You appear to have another instance of ethereum"
               "running. Bailing.";
      BOOST_THROW_EXCEPTION(DatabaseAlreadyOpen());
    }
  }
}

template <typename Pos>
BytesMap rlpArray(Pos size,
                  function<void(Pos const&, RLPStream&)> write_element) {
  BytesMap ret;
  for (auto i = 0; i < size; ++i) {
    RlpStream pos_rlp, element_rlp;
    pos_rlp << i;
    write_element(i, element_rlp);
    ret.insert(pos_rlp.out(), element_rlp.out());
  }
  return ret;
}

}  // namespace taraxa::util_eth

#endif  // TARAXA_NODE_UTIL_ETH_HPP
