//
// Created by JC on 2019-04-28.
//

#ifndef TARAXA_NODE_SIMPLEOVERLAYDBDELEGATE_H
#define TARAXA_NODE_SIMPLEOVERLAYDBDELEGATE_H

#include "libdevcore/OverlayDB.h"
#include "SimpleDBFace.h"

class SimpleOverlayDBDelegate : public SimpleDBFace {
public:
    bool put (const std::string &key, const std::string &value) override;
    bool update (const std::string &key, const std::string &value) override;
    std::string get  (const std::string &key) override;
    void commit  () override;
    SimpleOverlayDBDelegate(const std::string& path);
private:
    static h256 stringToHashKey (const std::string& s) {
      return h256(s);
    }

    std::shared_ptr<dev::OverlayDB> odb = nullptr;
};
#endif //TARAXA_NODE_SIMPLEOVERLAYDBDELEGATE_H
