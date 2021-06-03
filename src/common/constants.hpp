#pragma once

#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>

namespace taraxa {

inline static h256 const ZeroHash;
inline static auto const EmptySHA3 = dev::sha3(dev::bytesConstRef());
inline static auto const EmptyRLPListSHA3 = dev::sha3(dev::RLPStream(0).out());

}  // namespace taraxa