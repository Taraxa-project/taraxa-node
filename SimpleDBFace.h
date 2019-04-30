//
// Created by JC on 2019-04-26.
//
#ifndef TARAXA_NODE_SIMPLEDBFACE_H
#define TARAXA_NODE_SIMPLEDBFACE_H
#include "libethereum/State.h"

using h256 = dev::h256;
const h256 TEMP_GENESIS_HASH = h256{}; /* genesisHash, put h256{} for now */

class SimpleDBFace {
public:
    // @return if key exist, return false without updating value.
    virtual bool put(const std::string &key, const std::string &value) = 0;
    virtual bool update(const std::string &key, const std::string &value) = 0;
    // @return if key not exist, return "".
    virtual std::string get(const std::string &key) = 0;
    virtual void commit() = 0;
    virtual ~SimpleDBFace() = default;
};
#endif //TARAXA_NODE_TEST_H