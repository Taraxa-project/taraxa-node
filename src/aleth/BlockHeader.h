#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>

#include <algorithm>

#include "Common.h"
#include "Exceptions.h"
#include "types.hpp"

namespace taraxa::aleth {

enum BlockDataType { HeaderData, BlockData };

struct BlockHeaderFields {
  h256 m_parentHash;
  h256 m_sha3Uncles;
  h256 m_stateRoot;
  h256 m_transactionsRoot;
  h256 m_receiptsRoot;
  LogBloom m_logBloom;
  BlockNumber m_number = 0;
  uint64_t m_gasLimit = 0;
  uint64_t m_gasUsed = 0;
  bytes m_extraData;
  uint64_t m_timestamp = 0;
  Address m_author;
  u256 m_difficulty;
  h256 m_mixHash;
  Nonce m_nonce;

  void streamRLP(dev::RLPStream& _s) const;
};

class BlockHeader : BlockHeaderFields {
  mutable h256 m_hash;  ///< (Memoised) SHA3 hash of the block header with seal.

 public:
  BlockHeader(dev::RLP const& rlp);
  BlockHeader(BlockHeaderFields fields = {}, h256 const& hash = {})
      : BlockHeaderFields(std::move(fields)), m_hash(hash){};
  explicit BlockHeader(bytesConstRef _data, BlockDataType _bdt = BlockData)
      : BlockHeader(_bdt == BlockData ? dev::RLP(_data)[0] : dev::RLP(_data)) {}
  explicit BlockHeader(bytes const& _data, BlockDataType _bdt = BlockData) : BlockHeader(&_data, _bdt) {}

  h256 const& hash() const;
  BlockHeaderFields const& fields() const { return *this; }

  h256 const& parentHash() const { return m_parentHash; }
  h256 const& sha3Uncles() const { return m_sha3Uncles; }
  bool hasUncles() const { return m_sha3Uncles != dev::EmptyListSHA3; }
  auto timestamp() const { return m_timestamp; }
  Address const& author() const { return m_author; }
  h256 const& stateRoot() const { return m_stateRoot; }
  h256 const& transactionsRoot() const { return m_transactionsRoot; }
  h256 const& receiptsRoot() const { return m_receiptsRoot; }
  auto const& gasUsed() const { return m_gasUsed; }
  BlockNumber number() const { return m_number; }
  auto const& gasLimit() const { return m_gasLimit; }
  bytes const& extraData() const { return m_extraData; }
  LogBloom const& logBloom() const { return m_logBloom; }
  u256 const& difficulty() const { return m_difficulty; }
  auto const& mixHash() const { return m_mixHash; }
  auto const& nonce() const { return m_nonce; }

  using BlockHeaderFields::streamRLP;
};

}  // namespace taraxa::aleth
