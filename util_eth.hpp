#ifndef TARAXA_NODE_UTIL_ETH_HPP
#define TARAXA_NODE_UTIL_ETH_HPP

#include <libdevcore/Assertions.h>
#include <libdevcore/DBFactory.h>
#include <boost/filesystem.hpp>
#include <tuple>

namespace taraxa::util::eth::__impl__ {
using namespace std;
using namespace dev;
using namespace dev::db;
using namespace dev::eth;
namespace fs = boost::filesystem;

namespace exports {

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

struct RawDBAndMeta {
  //  RawDBAndMeta(RawDBAndMeta&&) = default;
  //  RawDBAndMeta& operator=(RawDBAndMeta&&) = default;
  shared_ptr<DatabaseFace> dbPtr;
  DatabaseKind kind;
  fs::path path;
};

struct OverlayAndRawDB {
  OverlayDB overlayDB;
  RawDBAndMeta rawDB;
};

// partially copied from /libethereum/State.cpp
inline OverlayAndRawDB newOverlayDB(fs::path const& _basePath,
                                    h256 const& _genesisHash, WithExisting _we,
                                    DatabaseKind kind = databaseKind()) {
  const auto isDiskDB = isDiskDatabase(kind);
  auto path = _basePath.empty() ? db::databasePath() : _basePath;
  if (isDiskDB && _we == WithExisting::Kill) {
    clog(VerbosityDebug, "statedb")
        << "Killing state database (WithExisting::Kill).";
    fs::remove_all(path / fs::path("state"));
  }
  path /= fs::path(toHex(_genesisHash.ref().cropped(0, 4))) /
          fs::path(toString(9 + (23 << 9)));  // copied from libethcore/Common.c
  if (isDiskDB) {
    fs::create_directories(path);
    DEV_IGNORE_EXCEPTIONS(fs::permissions(path, fs::owner_all));
  }
  path = path / fs::path("state");
  try {
    auto db = DBFactory::create(kind, path);
    clog(VerbosityTrace, "statedb") << "Opened state DB.";
    shared_ptr<DatabaseFace> dbRawPtr(db.get(), [](auto ptr) {});
    return {OverlayDB(move(db)), RawDBAndMeta{move(dbRawPtr), kind, path}};
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

}  // namespace exports
}  // namespace taraxa::util::eth::__impl__

namespace taraxa::util::eth {
using namespace __impl__::exports;
}

#endif  // TARAXA_NODE_UTIL_ETH_HPP
