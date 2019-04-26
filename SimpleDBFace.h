//
// Created by JC on 2019-04-26.
//
#include "rocks_db.hpp"
#include "libdevcore/OverlayDB.h"
#include "libethereum/State.h"
#include "types.hpp"

using OverlayDB = dev::OverlayDB;
using State = dev::eth::State;
using h256 = dev::h256;

const h256 TEMP_GENESIS_HASH = h256{}; /* genesisHash, put h256{} for now */

class SimpleDBFace {
public:
    enum SimpleDBType {
        TaraxaRocksDBKind,
        OverlayDBKind,
        StateDBKind
    };
    static std::shared_ptr<SimpleDBFace> createShared(SimpleDBFace::SimpleDBType, const std::string &);

    bool put(const std::string &key, const std::string &value);
    bool update(const std::string &key, const std::string &value);
    std::string get(const std::string &key);
    void commit();
    SimpleDBFace(SimpleDBType type, std::string path);
private:
    std::shared_ptr<dev::eth::State> state = nullptr;
    std::shared_ptr<taraxa::RocksDb> taraxa_rocks_db = nullptr;
    std::shared_ptr<dev::OverlayDB> odb = nullptr;
    const SimpleDBType type;
};