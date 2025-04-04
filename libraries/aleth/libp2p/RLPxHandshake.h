// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2015-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#pragma once

#include <libdevcrypto/Common.h>

#include <memory>

#include "Common.h"
#include "RLPXFrameCoder.h"
#include "RLPXSocket.h"

namespace dev {
namespace p2p {

/**
 * @brief Setup inbound or outbound connection for communication over
 * RLPXFrameCoder. RLPx Spec:
 * https://github.com/ethereum/devp2p/blob/master/rlpx.md#encrypted-handshake
 *
 * @todo Implement StartSession transition via lambda which is passed to
 * constructor.
 *
 * Thread Safety
 * Distinct Objects: Safe.
 * Shared objects: Unsafe.
 */
struct RLPXHandshake final : std::enable_shared_from_this<RLPXHandshake> {
  friend class RLPXFrameCoder;

  struct HostContext {
    KeyPair key_pair;
    unsigned port;
    std::string client_version;
    CapDescs capability_descriptions;
    std::function<void(Public const&, RLP const&, std::unique_ptr<RLPXFrameCoder>, std::shared_ptr<RLPXSocket> const&)>
        on_success;
    std::function<void(NodeID const&, HandshakeFailureReason)> on_failure;
  };
  /// Setup incoming connection.
  RLPXHandshake(std::shared_ptr<HostContext const> ctx, std::shared_ptr<RLPXSocket> const& _socket);

  /// Setup outbound connection.
  RLPXHandshake(std::shared_ptr<HostContext const> ctx, std::shared_ptr<RLPXSocket> const& _socket, NodeID _remote);

  /// Start handshake.
  void start() { transition(); }

  /// Aborts the handshake.
  void cancel();

  NodeID remote() const { return m_remote; }

 private:
  /// Timeout for a stage in the handshake to complete (the remote to respond to
  /// transition events). Enforced by m_idleTimer and refreshed by transition().
  static constexpr std::chrono::milliseconds c_timeout{1800};

  /// Sequential states of handshake
  enum State { Error = -1, New, AckAuth, AckAuthEIP8, WriteHello, ReadHello, StartSession };

  /// Write Auth message to socket and transitions to AckAuth.
  void writeAuth();

  /// Reads Auth message from socket and transitions to AckAuth.
  void readAuth();

  /// Continues reading Auth message in EIP-8 format and transitions to
  /// AckAuthEIP8.
  void readAuthEIP8();

  /// Derives ephemeral secret from signature and sets members after Auth has
  /// been decrypted.
  void setAuthValues(Signature const& sig, Public const& remotePubk, h256 const& remoteNonce, uint64_t remoteVersion);

  /// Write Ack message to socket and transitions to WriteHello.
  void writeAck();

  /// Write Ack message in EIP-8 format to socket and transitions to WriteHello.
  void writeAckEIP8();

  /// Reads Auth message from socket and transitions to WriteHello.
  void readAck();

  /// Continues reading Ack message in EIP-8 format and transitions to
  /// WriteHello.
  void readAckEIP8();

  /// Closes connection and ends transitions.
  void error(boost::system::error_code _ech = {});

  /// Performs transition for m_nextState.
  void transition(boost::system::error_code _ech = {});

  /// Get a string indicating if the connection is incoming or outgoing
  inline char const* connectionDirectionString() const { return m_originated ? "egress" : "ingress"; }

  /// Determine if the remote socket is still connected
  bool remoteSocketConnected() const;

  State m_nextState = New;  ///< Current or expected state of transition.
  bool m_cancel = false;    ///< Will be set to true if connection was canceled.

  std::shared_ptr<HostContext const> host_ctx_;

  /// Node id of remote host for socket.
  NodeID m_remote;            ///< Public address of remote host.
  bool m_originated = false;  ///< True if connection is outbound.

  /// Buffers for encoded and decoded handshake phases
  bytes m_auth;                ///< Plaintext of egress or ingress Auth message.
  bytes m_authCipher;          ///< Ciphertext of egress or ingress Auth message.
  bytes m_ack;                 ///< Plaintext of egress or ingress Ack message.
  bytes m_ackCipher;           ///< Ciphertext of egress or ingress Ack message.
  bytes m_handshakeOutBuffer;  ///< Frame buffer for egress Hello packet.
  bytes m_handshakeInBuffer;   ///< Frame buffer for ingress Hello packet.

  KeyPair m_ecdheLocal = KeyPair::create();  ///< Ephemeral ECDH secret and agreement.
  h256 m_nonce;                              ///< Nonce generated by this host for handshake.

  Public m_ecdheRemote;  ///< Remote ephemeral public key.
  h256 m_remoteNonce;    ///< Nonce generated by remote host for handshake.
  uint64_t m_remoteVersion;

  /// Used to read and write RLPx encrypted frames for last step of handshake
  /// authentication. Passed onto Host which will take ownership.
  std::unique_ptr<RLPXFrameCoder> m_io;

  std::shared_ptr<RLPXSocket> m_socket;

  /// Timer which enforces c_timeout. Reset for each stage of the handshake.
  ba::steady_timer m_idleTimer;

  HandshakeFailureReason m_failureReason;

  Logger m_logger{createLogger(VerbosityTrace, "rlpx")};
  Logger m_errorLogger{createLogger(VerbosityError, "rlpx")};
};

}  // namespace p2p
}  // namespace dev
