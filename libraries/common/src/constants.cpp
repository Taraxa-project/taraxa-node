#include "common/constants.hpp"

#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>

namespace taraxa {

GLOBAL_CONST_DEF(ZeroHash, {})
GLOBAL_CONST_DEF(EmptySHA3, dev::sha3(dev::bytesConstRef()))
GLOBAL_CONST_DEF(EmptyRLPListSHA3, dev::sha3(dev::RLPStream(0).out()))
GLOBAL_CONST_DEF(EmptyNonce, {})
GLOBAL_CONST_DEF(ZeroU256, {})

}  // namespace taraxa