#pragma once

#include <stdexcept>
#include <string>

#include "libp2p/Common.h"

namespace taraxa {

class PacketProcessingException : public std::runtime_error {
 public:
  PacketProcessingException(const std::string &msg, dev::p2p::DisconnectReason disconnect_reason)
      : runtime_error("PacketProcessingException -> " + msg), disconnect_reason_(disconnect_reason) {}

  dev::p2p::DisconnectReason getDisconnectReason() const { return disconnect_reason_; }

 private:
  const dev::p2p::DisconnectReason disconnect_reason_;
};

/**
 * @brief Exception thrown in case basic rlp format validation fails - number of rlp items
 * @note In case actual parsing fails due to types mismatch, one of thev::RLPException is thrown
 */
class InvalidRlpItemsCountException : public PacketProcessingException {
 public:
  InvalidRlpItemsCountException(const std::string &packet_type_str, size_t actual_size, size_t expected_size)
      : PacketProcessingException(packet_type_str + " RLP items count(" + std::to_string(actual_size) +
                                      "), expected size is " + std::to_string(expected_size),
                                  dev::p2p::DisconnectReason::BadProtocol) {}
};

/**
 * @brief Exception thrown in case peer seems malicious based on data he sent
 */
class MaliciousPeerException : public PacketProcessingException {
 public:
  MaliciousPeerException(const std::string &msg, std::optional<dev::p2p::NodeID> peer = {})
      : PacketProcessingException("MaliciousPeer " + (peer ? peer->abridged() : "") + ": " + msg,
                                  dev::p2p::DisconnectReason::UserReason),
        peer_(std::move(peer)) {}

  /**
   * @return peer that should be disconnected in case it was set
   */
  std::optional<dev::p2p::NodeID> getPeer() const { return peer_; }

 private:
  // Peer that should be disconnected - sometimes we want to disconnect data author, not packet sender
  std::optional<dev::p2p::NodeID> peer_;
};

}  // namespace taraxa