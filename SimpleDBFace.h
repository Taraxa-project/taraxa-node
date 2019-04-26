//
// Created by JC on 2019-04-26.
//

#include "rocks_db.hpp"
#include "libdevcore/OverlayDB.h"
#include "libethereum/State.h"

class SimpleDBFace {
    using dev::OverlayDB;
    using dev::eth::State;
    SimpleDBFace() {
      
    }
private:
    std::shared_ptr<State> state = nullptr;
    std::shared_ptr<RocksDb> db = nullptr;
    std::shared_ptr<OverlayDB> odb= nullptr;
};