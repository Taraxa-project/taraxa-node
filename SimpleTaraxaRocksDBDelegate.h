//
// Created by JC on 2019-04-27.
//

#ifndef TARAXA_NODE_SIMPLETARAXAROCKSDBDELEGATE_H
#define TARAXA_NODE_SIMPLETARAXAROCKSDBDELEGATE_H

#include "SimpleDBFace.h"

class SimpleTaraxaRocksDBDelegate : public SimpleDBFace {
public:
    bool put (const std::string &key, const std::string &value) override;
    bool update (const std::string &key, const std::string &value) override;
    std::string get  (const std::string &key) override;
    void commit  () override;
    SimpleTaraxaRocksDBDelegate(const std::string& path);
private:
    std::shared_ptr<taraxa::RocksDb> taraxa_rocks_db = nullptr;

    //static shared_ptr<SimpleDBFace> makeShared(const std::string& path) {
      //return make_shared<SimpleTaraxaRocksDBDelegate>(path);
    //}
    //SimpleTaraxaRocksDBDelegate(const std::string& path):taraxa_rocks_db(std::make_shared<taraxa::RocksDb>(path)){}
};


#endif //TARAXA_NODE_SIMPLETARAXAROCKSDBDELEGATE_H
