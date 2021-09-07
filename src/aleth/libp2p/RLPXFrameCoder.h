// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2015-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#pragma once

#include <libdevcore/RLP.h>
#include <libdevcrypto/CryptoPP.h>

#include <memory>

#include "Common.h"

namespace dev {
namespace p2p {

/**
 * @brief Encapsulation of Frame
 * @todo coder integration; padding derived from coder
 */
struct RLPXFrameInfo {
  /// Constructor. frame-size || protocol-type, [sequence-id[,
  /// total-packet-size]]
  explicit RLPXFrameInfo(bytesConstRef _frameHeader);

  uint32_t const length = 0;  ///< Size of frame (excludes padding). Max: 2**24
  uint8_t const padding = 0;  ///< Length of padding which follows @length.

  bytes const data;  ///< Bytes of Header.
  RLP const header;  ///< Header RLP.

  uint16_t const protocolId = 0;   ///< Protocol ID as negotiated by handshake.
  bool const multiFrame = 0;       ///< If this frame is part of a sequence
  uint16_t const sequenceId = 0;   ///< Sequence ID of frame
  uint32_t const totalLength = 0;  ///< Total length of packet in first frame of
                                   ///< multiframe packet.
};

struct RLPXHandshake;

/**
 * @brief Encoder/decoder transport for RLPx connection established by
 * RLPXHandshake.
 *
 * @todo rename to RLPXTranscoder
 * @todo Remove 'Frame' nomenclature and expect caller to provide RLPXFrame
 * @todo Remove handshake as friend, remove handshake-based constructor
 *
 * Thread Safety
 * Distinct Objects: Unsafe.
 * Shared objects: Unsafe.
 */

class RLPXFrameCoder {
  static constexpr size_t MAX_PACKET_SIZE = 15 * 1024 * 1024;

  friend struct Session;

  enum class ProtocolIdType : uint16_t { Normal = 0, Compressed };

 public:
  /// Construct; requires instance of RLPXHandshake which has encrypted ECDH key
  /// exchange (first two phases of handshake).
  explicit RLPXFrameCoder(RLPXHandshake const& _init);

  /// Construct with external key material.
  RLPXFrameCoder(bool _originated, h512 const& _remoteEphemeral, h256 const& _remoteNonce, KeyPair const& _ecdheLocal,
                 h256 const& _nonce, bytesConstRef _ackCipher, bytesConstRef _authCipher);

  ~RLPXFrameCoder();

  /// Legacy. Encrypt _packet as ill-defined legacy RLPx frame.
  void writeSingleFramePacket(bytesConstRef _packet, bytes& o_bytes);

  /// Authenticate and decrypt header in-place.
  bool authAndDecryptHeader(bytesRef io_cipherWithMac);

  /// Authenticate and decrypt frame in-place.
  bool authAndDecryptFrame(bytesRef io_cipherWithMac);

  /// Return first 16 bytes of current digest from egress mac.
  h128 egressDigest();

  /// Return first 16 bytes of current digest from ingress mac.
  h128 ingressDigest();

 private:
  /// Write continuation frame of segmented payload.
  void writeFrame(uint16_t _seqId, bytesConstRef _payload, bytes& o_bytes);

  void writeFrame(bytesConstRef _header, bytesConstRef _payload, bytes& o_bytes);
  /// Compression
  uint32_t decompressFrame(bytesRef payload, bytes& output) const;

  void LZ4compress(bytesConstRef payload, bytes& output) const;

  void writeCompressedFrame(uint16_t _seqId, bytesConstRef _payload, bytes& o_bytes);

  void writeCompressedFrame(uint16_t _seqId, uint32_t _totalSize, bytesConstRef _payload, bytes& o_bytes);
  // Compression <--- end

  /// Update state of egress MAC with frame header.
  void updateEgressMACWithHeader(bytesConstRef _headerCipher);

  /// Update state of egress MAC with frame.
  void updateEgressMACWithFrame(bytesConstRef _cipher);

  /// Update state of ingress MAC with frame header.
  void updateIngressMACWithHeader(bytesConstRef _headerCipher);

  /// Update state of ingress MAC with frame.
  void updateIngressMACWithFrame(bytesConstRef _cipher);

  /// Establish shared secrets and setup AES and MAC states.
  void setup(bool _originated, h512 const& _remoteEphemeral, h256 const& _remoteNonce, KeyPair const& _ecdheLocal,
             h256 const& _nonce, bytesConstRef _ackCipher, bytesConstRef _authCipher);

  std::unique_ptr<class RLPXFrameCoderImpl> m_impl;
};

}  // namespace p2p
}  // namespace dev