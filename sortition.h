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
#include "libdevcore/Log.h"

#include <string>

// max signature hash 64 hex "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
// change to decimal should be "115792089237316195423570985008687907853269984665640564039457584007913129639935"
#define SIGNATURE_HASH_MAX  "115792089237316195423570985008687907853269984665640564039457584007913129639935"
#define SIGNATURE_HASH_SIZE_MAX 78

// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define TARAXA_COINS "9007199254740991"
#define TARASA_COINS_DECIMAL 9007199254740991

namespace taraxa {
using std::string;

string hashSignature(dev::Signature signature);

bool sortition(string signature, uint64_t account_balance, size_t threshold);

string hexToDecimal(string hex);

string bigNumberMultiplication(string num1, string num2);

static dev::Logger log_sil_{
  dev::createLogger(dev::Verbosity::VerbositySilent, "SORTITION")};
static dev::Logger log_err_{
  dev::createLogger(dev::Verbosity::VerbosityError, "SORTITION")};
static dev::Logger log_war_{
  dev::createLogger(dev::Verbosity::VerbosityWarning, "SORTITION")};
static dev::Logger log_inf_{
  dev::createLogger(dev::Verbosity::VerbosityInfo, "SORTITION")};
static dev::Logger log_deb_{
  dev::createLogger(dev::Verbosity::VerbosityDebug, "SORTITION")};
static dev::Logger log_tra_{
  dev::createLogger(dev::Verbosity::VerbosityTrace, "SORTITION")};

} // namespace taraxa

#endif  // SORTITION_H
