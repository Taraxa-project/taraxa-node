// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include <libdevcore/Guards.h>
#include <libdevcrypto/Common.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "Common.h"
#include "ENR.h"
#include "Network.h"
#include "NodeTable.h"
#include "Peer.h"
#include "RLPxHandshake.h"
#include "Session.h"
#include "taraxa.hpp"

namespace io = boost::asio;
namespace bi = io::ip;

namespace std {
template <>
struct hash<pair<dev::p2p::NodeID, string>> {
  size_t operator()(pair<dev::p2p::NodeID, string> const& _value) const {
    size_t ret = hash<dev::p2p::NodeID>()(_value.first);
    return ret ^ (hash<string>()(_value.second) + 0x9e3779b9 + (ret << 6) + (ret >> 2));
  }
};
}  // namespace std

namespace dev {

namespace p2p {
class CapabilityFace;
struct Host;
struct Session;
class RLPXFrameCoder;

struct NodeInfo {
  NodeInfo() = default;
  NodeInfo(NodeID const& _id, std::string const& _address, unsigned _port, std::string const& _version,
           std::string const& _enr)
      : id(_id), address(_address), port(_port), version(_version), enr(_enr) {}

  std::string enode() const { return "enode://" + id.hex() + "@" + address + ":" + toString(port); }

  NodeID id;
  std::string address;
  unsigned port;
  std::string version;
  std::string enr;
};

/**
 * @brief The Host class
 * Capabilities should be registered prior to startNetwork, since m_capabilities
 * is not thread-safe.
 *
 * @todo determinePublic: ipv6, udp
 * @todo per-session keepalive/ping instead of broadcast; set ping-timeout via
 * median-latency
 */
struct Host final : std::enable_shared_from_this<Host> {
  using CapabilityList = std::vector<std::shared_ptr<CapabilityFace>>;
  using CapabilitiesFactory = std::function<CapabilityList(std::weak_ptr<Host>)>;

 private:
  Host(std::string _clientVersion, KeyPair const& kp, NetworkConfig _n, TaraxaNetworkConfig taraxa_conf,
       std::filesystem::path state_file_path);

 public:
  static std::shared_ptr<Host> make(std::string _clientVersion, CapabilitiesFactory const& cap_factory,
                                    KeyPair const& kp, NetworkConfig _n, TaraxaNetworkConfig taraxa_conf = {},
                                    std::filesystem::path state_file_path = {});

  std::shared_ptr<CapabilityFace> latestCapability() { return m_capabilities.rbegin()->second.ref; }
  ~Host();

  ba::io_context::count_type do_work();

  // This is only for discovery !!!!
  ba::io_context::count_type do_discov();

  bool isRunning() { return !ioc_.stopped(); }

  uint64_t peer_count() const { return peer_count_snapshot_; }
  /// Get the port we're listening on currently.
  unsigned short listenPort() const { return m_listenPort; }
  /// Get our current node ID.
  NodeID id() const { return m_alias.pub(); }

  auto getNodeCount() const { return m_nodeTable->count(); }

  std::list<NodeEntry> getNodes() const { return m_nodeTable->snapshot(); }

  void addNode(Node const& _node);

  void invalidateNode(NodeID const& _node);

  void disconnect(NodeID const& _nodeID, DisconnectReason _reason) {
    ba::post(strand_, [=, this] {
      if (auto session = peerSession(_nodeID)) {
        session->disconnect(_reason);
      }
    });
  }

  void send(NodeID const& node_id, std::string capability_name, unsigned packet_type, bytes payload,
            std::function<void()>&& on_done = {}) {
    ba::post(strand_, [=, this, capability_name = std::move(capability_name), payload = std::move(payload),
                       on_done = std::move(on_done)]() mutable {
      if (auto session = peerSession(node_id)) {
        session->send(std::move(capability_name), packet_type, std::move(payload), std::move(on_done));
      }
    });
  }

  /// Get the endpoint information.
  std::string enode() const {
    std::string address;
    if (!m_netConfig.publicIPAddress.empty())
      address = m_netConfig.publicIPAddress;
    else if (!m_tcpPublic.address().is_unspecified())
      address = m_tcpPublic.address().to_string();
    else
      address = c_localhostIp;

    std::string port;
    if (m_tcpPublic.port())
      port = toString(m_tcpPublic.port());
    else
      port = toString(m_netConfig.listenPort);

    return "enode://" + id().hex() + "@" + address + ":" + port;
  }

  bool nodeTableHasNode(Public const& _id) const;
  // private but can be made public if needed
 private:
  Node nodeFromNodeTable(Public const& _id) const;

  struct KnownNode {
    Node node;
    uint32_t lastPongReceivedTime;
    uint32_t lastPongSentTime;
  };
  bool addKnownNodeToNodeTable(KnownNode const& node);

  bool haveCapability(CapDesc const& _name) const { return m_capabilities.count(_name) != 0; }

  /// Get the address we're listening on currently.
  std::string listenAddress() const {
    return m_tcpPublic.address().is_unspecified() ? "0.0.0.0" : m_tcpPublic.address().to_string();
  }

  NetworkConfig const& networkConfig() const { return m_netConfig; }

  /// Get the public TCP endpoint.
  bi::tcp::endpoint const& tcpPublic() const { return m_tcpPublic; }

  /// Get the node information.
  p2p::NodeInfo nodeInfo() const {
    auto const e = enr();
    return NodeInfo(
        id(),
        (networkConfig().publicIPAddress.empty() ? m_tcpPublic.address().to_string() : networkConfig().publicIPAddress),
        m_tcpPublic.port(), m_clientVersion, e.textEncoding());
  }

  /// Get Ethereum Node Record of the host
  ENR enr() const { return m_nodeTable->hostENR(); }

  /// Get number of peers connected.
  size_t peer_count_() const;

  // Really private
 private:
  /// Get peer information.
  PeerSessionInfos peerSessionInfos() const;

  /// Get session by id
  std::shared_ptr<Session> peerSession(NodeID const& _id) const;

  /// Validates and starts peer session, taking ownership of _io. Disconnects
  /// and returns false upon error.
  void startPeerSession(Public const& _id, RLP const& _hello, std::unique_ptr<RLPXFrameCoder> _io,
                        std::shared_ptr<RLPXSocket> const& _s);

  void onNodeTableEvent(NodeID const& _n, NodeTableEventType const& _e);

  /// Serialise the set of known peers.
  void save_state() const;
  /// Deserialise the data and populate the set of known peers.
  struct PersistentState {
    ENR enr;
    std::vector<KnownNode> known_nodes;
    std::vector<std::shared_ptr<Peer>> peers;
  };
  std::optional<PersistentState> restore_state();

  enum PeerSlotType { Egress, Ingress };

  unsigned peerSlots(PeerSlotType _type) const {
    return _type == Egress ? m_idealPeerCount : m_idealPeerCount * m_stretchPeers;
  }

  bool havePeerSession(NodeID const& _id) { return bool(peerSession(_id)); }

  bool isHandshaking(NodeID const& _id) const;

  /// Determines publicly advertised address.
  bi::tcp::endpoint determinePublic() const;

  ENR updateENR(ENR const& _restoredENR, bi::tcp::endpoint const& _tcpPublic, uint16_t const& _listenPort);

  void connect(std::shared_ptr<Peer> const& _p);

  /// Returns true if pending and connected peer count is less than maximum
  bool peerSlotsAvailable(PeerSlotType _type = Ingress);

  /// Ping the peers to update the latency information and disconnect peers
  /// which have timed out.
  void keepAlivePeers();

  /// Log count of active peers and information about each peer
  void logActivePeers();

  void runAcceptor();

  void main_loop_body();

  /// Get or create host's Ethereum Node record.
  std::pair<Secret, ENR> restoreENR(bytesConstRef _b, NetworkConfig const& _networkConfig);

  /// Determines if a node with the supplied endpoint should be included in or
  /// restored from the serialized network configuration data
  bool isAllowedEndpoint(NodeIPEndpoint const& _endpointToCheck) const {
    return dev::p2p::isAllowedEndpoint(m_netConfig.allowLocalDiscovery, _endpointToCheck);
  }

  std::shared_ptr<Peer> peer(NodeID const& _n) const;

  // THREAD SAFE STATE

  std::atomic<bool> fully_initialized_ = false;

  ba::io_context ioc_;
  ba::executor_work_guard<ba::io_context::executor_type> ioc_w_;

  ba::io_context session_ioc_;
  ba::executor_work_guard<ba::io_context::executor_type> session_ioc_w_;

  ba::io_context::strand strand_;
  ///< Listening acceptor.
  /// Timer which, when network is running,
  /// calls run() every c_timerInterval ms.
  bi::tcp::acceptor m_tcp4Acceptor;
  ba::steady_timer m_runTimer;

  // IMMUTABLE STATE

  std::filesystem::path state_file_path_;
  std::string m_clientVersion;  ///< Our version string.
  NetworkConfig m_netConfig;    ///< Network settings.
  TaraxaNetworkConfig taraxa_conf_;
  unsigned m_idealPeerCount = 0;  ///< Ideal number of peers to be connected to.
  unsigned m_stretchPeers = 0;    ///< Accepted connection multiplier (max peers = ideal*stretch).
  /// Each of the capabilities we support.
  Capabilities m_capabilities;
  unsigned m_listenPort;
  bi::tcp::endpoint m_tcpPublic;  ///< Our public listening endpoint.
  /// Alias for network communication.
  KeyPair m_alias;

  std::shared_ptr<RLPXHandshake::HostContext> handshake_ctx_;

  // MUTABLE STATE | ACCESS IS SERIALIZED

  /// The nodes to which we are currently connected. Used by host to service
  /// peer requests and keepAlivePeers and for shutdown. (see run()) Mutable
  /// because we flush zombie entries (null-weakptrs) as regular maintenance
  /// from a const method.

  std::set<Peer*> m_pendingPeerConns;  /// Used only by connect(Peer&) to limit
  /// concurrently connecting to same node.
  /// See connect(shared_ptr<Peer>const&).

  /// Shared storage of Peer objects. Peers are created or destroyed on demand
  /// by the Host. Active sessions maintain a shared_ptr to a Peer;
  std::unordered_map<NodeID, std::shared_ptr<Peer>> m_peers;

  mutable std::unordered_map<NodeID, std::weak_ptr<Session>> m_sessions;

  /// Pending connections. Completed handshakes are garbage-collected in run()
  /// (a handshake is complete when there are no more shared_ptrs in handlers)
  std::list<std::weak_ptr<RLPXHandshake>> m_connecting;

  std::chrono::steady_clock::time_point m_lastPing;  ///< Time we sent the last ping to all peers.

  /// When the last "active peers" message was logged - used to throttle
  /// logging to once every c_logActivePeersInterval seconds
  std::chrono::steady_clock::time_point m_lastPeerLogMessage;

  // MUTABLE STATE | THREAD SAFETY GUARANTEED BY THE CLASSES

  std::unique_ptr<NodeTable> m_nodeTable;  ///< Node table (uses kademlia-like discovery).

  std::atomic<uint64_t> peer_count_snapshot_ = 0;

  // LOGGERS ARE THREAD SAFE
  mutable Logger m_logger{createLogger(VerbosityDebug, "net")};
  Logger m_detailsLogger{createLogger(VerbosityTrace, "net")};
  Logger m_infoLogger{createLogger(VerbosityInfo, "net")};
};

}  // namespace p2p
}  // namespace dev
