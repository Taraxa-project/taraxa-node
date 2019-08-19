/*
 * @Copyright: Taraxa.io
 * @Author: Qi Gao
 * @Date: 2019-04-07
 * @Last Modified by: Qi Gao
 * @Last Modified time: 2019-08-15
 */

#ifndef SORTITION_H
#define SORTITION_H

#include <string>
#include "libdevcore/Log.h"
#include "libdevcrypto/Common.h"
#include "types.hpp"

// max signature hash 64 hex
// "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff" change to
// decimal should be
// "115792089237316195423570985008687907853269984665640564039457584007913129639935"
#define SIGNATURE_HASH_MAX                                                     \
  "11579208923731619542357098500868790785326998466564056403945758400791312963" \
  "9935"
#define SIGNATURE_HASH_SIZE_MAX 78

// total TARAXA COINS (2^53 -1) "1fffffffffffff"
#define TARAXA_COINS "9007199254740991"
#define TARAXA_COINS_DECIMAL 9007199254740991

namespace taraxa {
using std::string;

string hashSignature(dev::Signature signature);

bool sortition(string signature, val_t account_balance, size_t threshold);

string hexToDecimal(string hex);

string bigNumberMultiplication(string num1, string num2);

static dev::Logger log_silent_{
    dev::createLogger(dev::Verbosity::VerbositySilent, "SORTI")};
static dev::Logger log_error_{
    dev::createLogger(dev::Verbosity::VerbosityError, "SORTI")};
static dev::Logger log_warning_{
    dev::createLogger(dev::Verbosity::VerbosityWarning, "SORTI")};
static dev::Logger log_info_{
    dev::createLogger(dev::Verbosity::VerbosityInfo, "SORTI")};
static dev::Logger log_debug_{
    dev::createLogger(dev::Verbosity::VerbosityDebug, "SORTI")};
static dev::Logger log_trace_{
    dev::createLogger(dev::Verbosity::VerbosityTrace, "SORTI")};

}  // namespace taraxa

#endif  // SORTITION_H
