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
  dev::p2p::DisconnectReason disconnect_reason_;
};

/**
 * @brief Exception thrown in case basic rlp format validation fails - number of rlp items
 * @note In case actual parsing fails due to types mismatch, one of thev::RLPException is thrown
 */
class InvalidRlpItemsCountException : public PacketProcessingException {
 public:
  InvalidRlpItemsCountException(const std::string &packet_type_str, size_t actual_size, size_t expected_size)
      : PacketProcessingException(packet_type_str + " RLP items count(" + std::to_string(actual_size) +
                                      ") != " + std::to_string(expected_size),
                                  dev::p2p::DisconnectReason::BadProtocol) {}
};

/**
 * @brief Exception thrown in case peer seems malicious based on data he sent
 */
class MaliciousPeerException : public PacketProcessingException {
 public:
  MaliciousPeerException(const std::string &msg)
      : PacketProcessingException("MaliciousPeer: " + msg, dev::p2p::DisconnectReason::UserReason) {}
};

}  // namespace taraxa