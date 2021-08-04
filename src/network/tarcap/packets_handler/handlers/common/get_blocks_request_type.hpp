#pragma once

namespace taraxa::network::tarcap {

enum GetBlocksPacketRequestType : ::byte { MissingHashes = 0x0, KnownHashes };

}  // namespace taraxa::network::tarcap