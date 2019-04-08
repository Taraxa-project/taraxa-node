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

// max signature hash 32 hex "ffffffffffffffffffffffffffffffff"
// change to decimal should be "340282366920938463463374607431768211455"
#define SIGNATURE_HASH_MAX  "340282366920938463463374607431768211455"
#define SIGNATURE_HASH_SIZE_MAX 39

// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define TARAXA_COINS "9007199254740991"

#define THRESHOLD 10

namespace taraxa {
using std::string;

string hash_signature(dev::Signature signature);

bool sortition(string signature, uint64_t account_balance);

string hex_to_decimal(string hex);

string big_number_multiplication(string num1, string num2);

} // namespace taraxa

#endif  // SORTITION_H
