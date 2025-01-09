#pragma once

#include <libdevcore/CommonJS.h>

#include "transaction/transaction.hpp"

namespace taraxa {

//{
// "blockHash":"0x6722e9c012f783c3d42e0e920b3c446f6c14273662eb063a794b1718da632a42",
// "blockNumber":"0xb31684",
// "from":"0xde2b1203d72d3549ee2f733b00b2789414c7cea5",
// "gas":"0x0",
// "gasPrice":"0x1",
// "hash":"0xdf374602b4f698a5ec6c8b2a9c27ac1b3190c093cd4ab5e624fe21caca6c24a1",
// "input":"0x",
// "nonce":"0x0",
// "r":"0xb48d8b4b64040e4bd585bf638a74a355be589251dc7458c0e5e563e7fe0d630a",
// "s":"0x57726ccf2fd4e97b297a212ed1f82beedd3c7b7326d07e12e50c87b331c60500",
// "to":"0x973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0",
// "transactionIndex":"0x6",
// "v":"0x1",
// "value":"0x0"
//}
const static auto kProblematicTrx = std::make_shared<Transaction>(dev::jsToBytes(
    "0xf85f80018094973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b080808206b6a0b48d8b4b64040e4bd585bf638a74a355be589251dc7458c0"
    "e5e563e7fe0d630aa057726ccf2fd4e97b297a212ed1f82beedd3c7b7326d07e12e50c87b331c60500"));
}  // namespace taraxa