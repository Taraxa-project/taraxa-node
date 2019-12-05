#ifndef TARAXA_NODE_SORTITION_H
#define TARAXA_NODE_SORTITION_H

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

namespace taraxa {
using std::string;

string hashSignature(dev::Signature signature);

bool sortition(string credential, size_t valid_players, size_t threshold);

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
