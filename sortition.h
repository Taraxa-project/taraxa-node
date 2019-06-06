/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-07
 * @Last Modified by:
 * @Last Modified time:
 */

#ifndef SORTITION_H
#define SORTITION_H

#include "libdevcrypto/Common.h"

#include <string>

// max signature hash 64 hex "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
// change to decimal should be "115792089237316195423570985008687907853269984665640564039457584007913129639935"
#define SIGNATURE_HASH_MAX  "115792089237316195423570985008687907853269984665640564039457584007913129639935"
#define SIGNATURE_HASH_SIZE_MAX 78

// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define TARAXA_COINS "9007199254740991"

#define THRESHOLD 3

namespace taraxa {
using std::string;

string hashSignature(dev::Signature signature);

bool sortition(string signature, uint64_t account_balance);

string hexToDecimal(string hex);

string bigNumberMultiplication(string num1, string num2);

} // namespace taraxa

#endif  // SORTITION_H
