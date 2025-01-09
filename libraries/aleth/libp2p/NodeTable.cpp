// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "NodeTable.h"

#include <cstdint>

namespace dev {
namespace p2p {
namespace {
// global thread-safe logger for static methods
BOOST_LOG_INLINE_GLOBAL_LOGGER_CTOR_ARGS(g_discoveryWarnLogger, boost::log::sources::severity_channel_logger_mt<>,
                                         (boost::log::keywords::severity = 0)(boost::log::keywords::channel = "discov"))

// Cadence at which we timeout sent pings and evict unresponsive nodes
constexpr std::chrono::milliseconds c_handleTimeoutsIntervalMs{5000};
// Cadence at which we remove old records from EndpointTracker
constexpr std::chrono::milliseconds c_removeOldEndpointStatementsIntervalMs{5000};
// Change external endpoint after this number of peers report new one
constexpr size_t c_minEndpointTrackStatements{10};
// Interval during which each endpoint statement is kept
constexpr std::chrono::minutes c_endpointStatementTimeToLiveMin{5};

}  // namespace

constexpr std::chrono::seconds DiscoveryDatagram::c_timeToLiveS;
constexpr std::chrono::milliseconds NodeTable::c_reqTimeoutMs;
constexpr std::chrono::milliseconds NodeTable::c_bucketRefreshMs;
constexpr std::chrono::milliseconds NodeTable::c_discoveryRoundIntervalMs;

inline bool operator==(std::weak_ptr<NodeEntry> const& _weak, std::shared_ptr<NodeEntry> const& _shared) {
  return !_weak.owner_before(_shared) && !_shared.owner_before(_weak);
}

NodeTable::NodeTable(ba::io_context& _io, KeyPair const& _alias, NodeIPEndpoint const& _endpoint, ENR const& _enr,
                     bool _enabled, bool _allowLocalDiscovery, bool is_boot_node, uint32_t chain_id)
    : strand_(ba::make_strand(_io)),
      m_hostNodeID{_alias.pub()},
      m_hostNodeIDHash{sha3(m_hostNodeID)},
      m_allowLocalDiscovery{_allowLocalDiscovery},
      m_hostStaticIP{isAllowedEndpoint(_endpoint) ? _endpoint.address() : bi::address{}},
      m_hostNodeEndpoint{_enr.ip(), _enr.udpPort(), _enr.tcpPort()},
      m_hostENR{_enr},
      m_secret{_alias.secret()},
      m_socket{make_shared<NodeSocket>(strand_, static_cast<UDPSocketEvents&>(*this), (bi::udp::endpoint)_endpoint)},
      m_requestTimeToLive{DiscoveryDatagram::c_timeToLiveS},
      m_discoveryTimer{make_shared<ba::steady_timer>(_io)},
      m_timeoutsTimer{make_shared<ba::steady_timer>(_io)},
      m_endpointTrackingTimer{make_shared<ba::steady_timer>(_io)},
      is_boot_node_(is_boot_node),
      chain_id_(chain_id) {
  if (is_boot_node_) {
    s_bucketSize = BOOT_NODE_BUCKET_SIZE;
  }
  for (unsigned i = 0; i < s_bins; i++) m_buckets[i].distance = i;

  if (!_enabled) {
    cwarn << "\"_enabled\" parameter is false, discovery is disabled";
    return;
  }
  m_socket->connect();
  doDiscovery();
  doHandleTimeouts();
  doEndpointTracking();
}

void NodeTable::processEvents() {
  if (m_nodeEventHandler) m_nodeEventHandler->processEvents();
}

bool NodeTable::addNode(Node const& _node) {
  LOG(m_logger) << "Adding node " << _node;

  if (!isValidNode(_node)) return false;

  bool needToPing = false;
  DEV_GUARDED(x_nodes) {
    auto const it = m_allNodes.find(_node.id);
    needToPing = (it == m_allNodes.end() || it->second->endpoint() != _node.get_endpoint() ||
                  !it->second->hasValidEndpointProof());
    if (needToPing && it != m_allNodes.end() &&
        it->second->lastPongReceivedTime > RLPXDatagramFace::secondsSinceEpoch() - m_requestTimeToLive.count()) {
      needToPing = false;
    }
  }

  if (needToPing) {
    LOG(m_logger) << "Pending " << _node;
    schedulePing(_node);
  }

  return true;
}

bool NodeTable::addKnownNode(Node const& _node, uint32_t _lastPongReceivedTime, uint32_t _lastPongSentTime) {
  LOG(m_logger) << "Adding known node " << _node;

  if (!isValidNode(_node)) return false;

  if (nodeEntry(_node.id)) {
    LOG(m_logger) << "Node " << _node << " is already in the node table";
    return true;
  }

  auto entry = std::make_shared<NodeEntry>(m_hostNodeIDHash, _node.id, _node.get_endpoint(), _lastPongReceivedTime,
                                           _lastPongSentTime);

  if (entry->hasValidEndpointProof()) {
    LOG(m_logger) << "Known " << _node;
    noteActiveNode(std::move(entry));
  } else {
    LOG(m_logger) << "Pending " << _node;
    schedulePing(_node);
  }

  return true;
}

bool NodeTable::isValidNode(Node const& _node) const {
  if (!_node.get_endpoint() || !_node.id) {
    LOG(m_logger) << "Supplied node " << _node
                  << " has an invalid endpoint or id. Skipping adding node to "
                     "node table.";
    return false;
  }

  if (!isAllowedEndpoint(_node.get_endpoint())) {
    LOG(m_logger) << "Supplied node" << _node
                  << " doesn't have an allowed endpoint. Skipping adding node "
                     "to node table";
    return false;
  }

  if (m_hostNodeID == _node.id) {
    LOG(m_logger) << "Skip adding self to node table (" << _node.id << ")";
    return false;
  }

  return true;
}

std::list<NodeID> NodeTable::nodes() const {
  std::list<NodeID> nodes;
  DEV_GUARDED(x_nodes) {
    for (auto& i : m_allNodes) nodes.push_back(i.second->id());
  }
  return nodes;
}

std::list<NodeEntry> NodeTable::snapshot() const {
  std::list<NodeEntry> ret;
  DEV_GUARDED(x_state) {
    for (auto const& s : m_buckets)
      for (auto const& np : s.nodes)
        if (auto n = np.lock()) ret.push_back(*n);
  }
  return ret;
}

Node NodeTable::node(NodeID const& _id) {
  Guard l(x_nodes);
  auto const it = m_allNodes.find(_id);
  if (it != m_allNodes.end()) {
    auto const& entry = it->second;
    return Node(_id, entry->endpoint(), entry->peerType());
  }
  return UnspecifiedNode;
}

std::shared_ptr<NodeEntry> NodeTable::nodeEntry(NodeID const& _id) {
  Guard l(x_nodes);
  auto const it = m_allNodes.find(_id);
  return it != m_allNodes.end() ? it->second : std::shared_ptr<NodeEntry>();
}

void NodeTable::doDiscoveryRound(NodeID _node, unsigned _round,
                                 std::shared_ptr<std::set<std::shared_ptr<NodeEntry>>> _tried) {
  // NOTE: ONLY called by doDiscovery or "recursively" via lambda scheduled via
  // timer at the end of this function
  if (!m_socket->isOpen()) return;

  // send s_alpha FindNode packets to nodes we know, closest to target
  auto const nearestNodes = nearestNodeEntries(_node);
  auto newTriedCount = 0;
  for (auto const& nodeEntry : nearestNodes) {
    if (!contains(*_tried, nodeEntry)) {
      // Avoid sending FindNode, if we have not sent a valid PONG lately.
      // This prevents being considered invalid node and FindNode being ignored.
      if (!nodeEntry->hasValidEndpointProof()) {
        LOG(m_logger) << "Node " << nodeEntry->node << " endpoint proof expired.";
        ping(nodeEntry->node);
        continue;
      }

      FindNode p(nodeEntry->endpoint(), _node);
      p.expiration = nextRequestExpirationTime();
      p.sign(m_secret);
      m_sentFindNodes.emplace_back(nodeEntry->id(), std::chrono::steady_clock::now());
      LOG(m_logger) << p.typeName() << " to " << nodeEntry->node << " (target: " << _node << ")";
      m_socket->send(p);

      _tried->emplace(nodeEntry);
      if (++newTriedCount == s_alpha) break;
    }
  }

  if (_round == s_maxSteps || newTriedCount == 0) {
    LOG(m_logger) << "Terminating discover after " << _round << " rounds.";
    doDiscovery();
    return;
  }

  m_discoveryTimer->expires_after(c_discoveryRoundIntervalMs);
  auto discoveryTimer{m_discoveryTimer};
  m_discoveryTimer->async_wait(boost::asio::bind_executor(
      strand_, [this, discoveryTimer, _node, _round, _tried](boost::system::error_code const& _ec) {
        // We can't use m_logger here if there's an error because captured this
        // might already be destroyed
        if (_ec.value() == boost::asio::error::operation_aborted || discoveryTimer->expiry() == c_steadyClockMin) {
          clog(VerbosityDebug, "discov") << "Discovery timer was probably cancelled";
          return;
        } else if (_ec) {
          clog(VerbosityDebug, "discov") << "Discovery timer error detected: " << _ec.value() << " " << _ec.message();
          return;
        }

        doDiscoveryRound(_node, _round + 1, _tried);
      }));
}

std::vector<std::shared_ptr<NodeEntry>> NodeTable::nearestNodeEntries(NodeID const& _target) {
  auto const distanceToTargetLess = [](std::pair<int, std::shared_ptr<NodeEntry>> const& _node1,
                                       std::pair<int, std::shared_ptr<NodeEntry>> const& _node2) {
    return _node1.first < _node2.first;
  };

  h256 const targetHash = sha3(_target);

  std::multiset<std::pair<int, std::shared_ptr<NodeEntry>>, decltype(distanceToTargetLess)> nodesByDistanceToTarget(
      distanceToTargetLess);
  for (auto const& bucket : m_buckets)
    for (auto const& nodeWeakPtr : bucket.nodes)
      if (auto node = nodeWeakPtr.lock()) {
        nodesByDistanceToTarget.emplace(distance(targetHash, node->nodeIDHash), node);

        if (nodesByDistanceToTarget.size() > NODE_BUCKET_SIZE)
          nodesByDistanceToTarget.erase(--nodesByDistanceToTarget.end());
      }

  std::vector<std::shared_ptr<NodeEntry>> ret;
  for (auto& distanceAndNode : nodesByDistanceToTarget) ret.emplace_back(std::move(distanceAndNode.second));

  return ret;
}

void NodeTable::ping(Node const& _node, std::shared_ptr<NodeEntry> _replacementNodeEntry) {
  if (!m_socket->isOpen()) return;

  // Don't send Ping if one is already sent
  if (m_sentPings.find(_node.get_endpoint()) != m_sentPings.end()) {
    LOG(m_logger) << "Ignoring request to ping " << _node << ", because it's already pinged";
    return;
  }

  PingNode p{m_hostNodeEndpoint, _node.get_endpoint(), chain_id_};
  p.expiration = nextRequestExpirationTime();
  p.seq = m_hostENR.sequenceNumber();
  auto const pingHash = p.sign(m_secret);
  LOG(m_logger) << p.typeName() << " to " << _node;
  m_socket->send(p);

  NodeValidation const validation{
      _node.id, _node.get_endpoint().tcpPort(), _node.get_endpoint().udpPort(), std::chrono::steady_clock::now(),
      pingHash, _replacementNodeEntry};
  m_sentPings.insert({_node.get_endpoint(), validation});
}

void NodeTable::schedulePing(Node const& _node) {
  post(strand_, [this, _node] { ping(_node, {}); });
}

void NodeTable::evict(NodeEntry const& _leastSeen, std::shared_ptr<NodeEntry> _replacement) {
  if (!m_socket->isOpen()) return;
  LOG(m_logger) << "Evicting node " << _leastSeen.node;
  ping(_leastSeen.node, std::move(_replacement));

  if (m_nodeEventHandler) m_nodeEventHandler->appendEvent(_leastSeen.id(), NodeEntryScheduledForEviction);
}

void NodeTable::noteActiveNode(std::shared_ptr<NodeEntry> _nodeEntry) {
  assert(_nodeEntry);

  if (_nodeEntry->id() == m_hostNodeID) {
    LOG(m_logger) << "Skipping making self active.";
    return;
  }
  if (!isAllowedEndpoint(_nodeEntry->endpoint())) {
    LOG(m_logger) << "Skipping making node with unallowed endpoint active. Node " << _nodeEntry->node;
    return;
  }

  if (!_nodeEntry->hasValidEndpointProof()) return;

  LOG(m_logger) << "Active node " << _nodeEntry->node;

  std::shared_ptr<NodeEntry> nodeToEvict;
  {
    Guard l(x_state);
    // Find a bucket to put a node to
    NodeBucket& s = bucket_UNSAFE(_nodeEntry.get());
    auto& nodes = s.nodes;

    // check if the node is already in the bucket
    auto it = find(nodes.begin(), nodes.end(), _nodeEntry);
    if (it != nodes.end()) {
      // if it was in the bucket, move it to the last position
      nodes.splice(nodes.end(), nodes, it);
    } else {
      if (nodes.size() < s_bucketSize) {
        // if it was not there, just add it as a most recently seen node
        // (i.e. to the end of the list)
        nodes.push_back(_nodeEntry);
        DEV_GUARDED(x_nodes) { m_allNodes.insert({_nodeEntry->id(), _nodeEntry}); }
        if (m_nodeEventHandler) m_nodeEventHandler->appendEvent(_nodeEntry->id(), NodeEntryAdded);
      } else {
        // if bucket is full, start eviction process for the least recently seen
        // node
        nodeToEvict = nodes.front().lock();
        // It could have been replaced in addNode(), then weak_ptr is expired.
        // If so, just add a new one instead of expired
        // TODO maybe remove is_boot_node_ part
        if (!nodeToEvict || is_boot_node_) {
          nodes.pop_front();
          nodes.push_back(_nodeEntry);
          DEV_GUARDED(x_nodes) { m_allNodes.insert({_nodeEntry->id(), _nodeEntry}); }
          if (m_nodeEventHandler) m_nodeEventHandler->appendEvent(_nodeEntry->id(), NodeEntryAdded);
          return;
        }
      }
    }
  }
  if (nodeToEvict) evict(*nodeToEvict, _nodeEntry);
}

void NodeTable::invalidateNode(NodeID const& _id) {
  auto sourceNodeEntry = nodeEntry(_id);
  if (!sourceNodeEntry) {
    return;
  }
  // This will cause the node to be pinged again to update status on the other
  // node
  sourceNodeEntry->lastPongReceivedTime = RLPXDatagramFace::secondsSinceEpoch() - NodeTable::c_bondingTimeSeconds;
}

void NodeTable::dropNode(std::shared_ptr<NodeEntry> _n) {
  // remove from nodetable
  {
    Guard l(x_state);
    NodeBucket& s = bucket_UNSAFE(_n.get());
    s.nodes.remove_if([_n](std::weak_ptr<NodeEntry> const& _bucketEntry) { return _bucketEntry == _n; });
  }

  DEV_GUARDED(x_nodes) { m_allNodes.erase(_n->id()); }
  DEV_GUARDED(x_ips) {
    m_ipMappings.erase(m_id2IpMap[_n->id()]);
    m_id2IpMap.erase(_n->id());
  }

  // notify host
  LOG(m_logger) << "p2p.nodes.drop " << _n->id();
  if (m_nodeEventHandler) m_nodeEventHandler->appendEvent(_n->id(), NodeEntryDropped);
}

NodeTable::NodeBucket& NodeTable::bucket_UNSAFE(NodeEntry const* _n) { return m_buckets[_n->distance - 1]; }

void NodeTable::onPacketReceived(UDPSocketFace*, bi::udp::endpoint const& _from, bytesConstRef _packet) {
  auto node_ip = _from;
  {
    Guard l(x_ips);
    if (m_ipMappings.contains(_from)) {
      node_ip = m_ipMappings[_from];
    }
  }
  try {
    std::unique_ptr<DiscoveryDatagram> packet = DiscoveryDatagram::interpretUDP(node_ip, _packet);
    if (!packet) return;
    if (packet->isExpired()) {
      LOG(m_logger) << "Expired " << packet->typeName() << " from " << packet->sourceid << "@" << node_ip;
      return;
    }
    LOG(m_logger) << packet->typeName() << " from " << packet->sourceid << "@" << node_ip;

    std::shared_ptr<NodeEntry> sourceNodeEntry;
    switch (packet->packetType()) {
      case Pong::type:
        sourceNodeEntry = handlePong(node_ip, *packet);
        break;

      case Neighbours::type:
        sourceNodeEntry = handleNeighbours(node_ip, *packet);
        break;

      case FindNode::type:
        sourceNodeEntry = handleFindNode(node_ip, *packet);
        break;

      case PingNode::type:
        sourceNodeEntry = handlePingNode(node_ip, *packet);
        break;

      case ENRRequest::type:
        sourceNodeEntry = handleENRRequest(node_ip, *packet);
        break;

      case ENRResponse::type:
        sourceNodeEntry = handleENRResponse(node_ip, *packet);
        break;
    }

    if (sourceNodeEntry) noteActiveNode(std::move(sourceNodeEntry));
  } catch (std::exception const& _e) {
    LOG(m_logger) << "Exception processing message from " << node_ip.address().to_string() << ":" << node_ip.port()
                  << ": " << _e.what();
  } catch (...) {
    LOG(m_logger) << "Exception processing message from " << node_ip.address().to_string() << ":" << node_ip.port();
  }
}

std::shared_ptr<NodeEntry> NodeTable::handlePong(bi::udp::endpoint const& _from, DiscoveryDatagram const& _packet) {
  // validate pong
  auto const sentPing = m_sentPings.find(_from);
  if (sentPing == m_sentPings.end()) {
    LOG(m_logger) << "Unexpected PONG from " << _from.address().to_string() << ":" << _from.port();
    return {};
  }

  auto const& pong = dynamic_cast<Pong const&>(_packet);
  auto const& nodeValidation = sentPing->second;
  if (pong.echo != nodeValidation.pingHash) {
    LOG(m_logger) << "Invalid PONG from " << _from.address().to_string() << ":" << _from.port();
    return {};
  }

  // in case the node answers with new NodeID, drop the record with the old
  // NodeID
  auto const& sourceId = pong.sourceid;
  if (sourceId != nodeValidation.nodeID) {
    LOG(m_logger) << "Node " << _from << " changed public key from " << nodeValidation.nodeID << " to " << sourceId;
    if (auto node = nodeEntry(nodeValidation.nodeID)) dropNode(std::move(node));
  }

  // create or update nodeEntry with new Pong received time
  std::shared_ptr<NodeEntry> sourceNodeEntry;
  DEV_GUARDED(x_nodes) {
    auto it = m_allNodes.find(sourceId);
    if (it == m_allNodes.end()) {
      sourceNodeEntry = std::make_shared<NodeEntry>(
          m_hostNodeIDHash, sourceId, NodeIPEndpoint{_from.address(), _from.port(), nodeValidation.tcpPort},
          RLPXDatagramFace::secondsSinceEpoch(), 0 /* lastPongSentTime */);

      // We need to setup external port, as we where able to do ping-pong exchange and node is active
      sourceNodeEntry->node.external_udp_port = nodeValidation.udpPort;
    } else {
      sourceNodeEntry = it->second;
      sourceNodeEntry->lastPongReceivedTime = RLPXDatagramFace::secondsSinceEpoch();

      if (sourceNodeEntry->endpoint() != _from)
        sourceNodeEntry->node.set_endpoint(NodeIPEndpoint{_from.address(), _from.port(), nodeValidation.tcpPort});
    }
  }

  m_sentPings.erase(_from);

  // update our external endpoint address and UDP port
  if (m_endpointTracker.addEndpointStatement(_from, pong.destination) >= c_minEndpointTrackStatements) {
    auto newUdpEndpoint = m_endpointTracker.bestEndpoint();
    if (!m_hostStaticIP.is_unspecified()) newUdpEndpoint.address(m_hostStaticIP);

    if (newUdpEndpoint != m_hostNodeEndpoint) {
      m_hostNodeEndpoint =
          NodeIPEndpoint{newUdpEndpoint.address(), newUdpEndpoint.port(), m_hostNodeEndpoint.tcpPort()};
      {
        Guard l(m_hostENRMutex);
        m_hostENR = IdentitySchemeV4::updateENR(m_hostENR, m_secret, m_hostNodeEndpoint.address(),
                                                m_hostNodeEndpoint.tcpPort(), m_hostNodeEndpoint.udpPort());
      }
      clog(VerbosityInfo, "net") << "ENR updated: " << m_hostENR;
    }
  }

  return sourceNodeEntry;
}

std::shared_ptr<NodeEntry> NodeTable::handleNeighbours(bi::udp::endpoint const& _from,
                                                       DiscoveryDatagram const& _packet) {
  std::shared_ptr<NodeEntry> sourceNodeEntry = nodeEntry(_packet.sourceid);
  if (!sourceNodeEntry) {
    LOG(m_logger) << "Source node (" << _packet.sourceid << "@" << _from
                  << ") not found in node table. Ignoring Neighbours packet.";
    return {};
  }
  if (sourceNodeEntry->endpoint() != _from) {
    LOG(m_logger) << "Neighbours packet from unexpected endpoint " << _from << " instead of "
                  << sourceNodeEntry->endpoint();
    return {};
  }

  auto const& in = dynamic_cast<Neighbours const&>(_packet);

  bool expected = false;
  auto now = std::chrono::steady_clock::now();
  m_sentFindNodes.remove_if([&](NodeIdTimePoint const& _t) noexcept {
    if (_t.first != in.sourceid) return false;
    if (now - _t.second < c_reqTimeoutMs) expected = true;
    return true;
  });
  if (!expected) {
    LOG(m_logger) << "Dropping unsolicited neighbours packet from " << _packet.sourceid << "@" << _from.address() << ":"
                  << _from.port();
    return sourceNodeEntry;
  }

  for (auto const& n : in.neighbours) addNode(Node(n.node, n.endpoint));

  return sourceNodeEntry;
}

std::shared_ptr<NodeEntry> NodeTable::handleFindNode(bi::udp::endpoint const& _from, DiscoveryDatagram const& _packet) {
  std::shared_ptr<NodeEntry> sourceNodeEntry = nodeEntry(_packet.sourceid);
  if (!sourceNodeEntry) {
    LOG(m_logger) << "Source node (" << _packet.sourceid << "@" << _from
                  << ") not found in node table. Ignoring FindNode request.";
    return {};
  }
  if (sourceNodeEntry->endpoint() != _from) {
    LOG(m_logger) << "FindNode packet from unexpected endpoint " << _from << " instead of "
                  << sourceNodeEntry->endpoint();
    return {};
  }
  if (!sourceNodeEntry->lastPongReceivedTime) {
    LOG(m_logger) << "Unexpected FindNode packet! Endpoint proof hasn't been "
                     "performed yet.";
    return {};
  }
  if (!sourceNodeEntry->hasValidEndpointProof()) {
    LOG(m_logger) << "Unexpected FindNode packet! Endpoint proof has expired.";
    return {};
  }

  auto const& in = dynamic_cast<FindNode const&>(_packet);
  std::vector<std::shared_ptr<NodeEntry>> nearest = nearestNodeEntries(in.target);
  static unsigned constexpr nlimit = (NodeSocket::maxDatagramSize - 109) / 90;
  for (unsigned offset = 0; offset < nearest.size(); offset += nlimit) {
    Neighbours out(_from, nearest, offset, nlimit);
    out.expiration = nextRequestExpirationTime();
    LOG(m_logger) << out.typeName() << " to " << in.sourceid << "@" << _from;
    out.sign(m_secret);
    if (out.data.size() > 1280) cnetlog << "Sending truncated datagram, size: " << out.data.size();
    m_socket->send(out);
  }

  return sourceNodeEntry;
}

NodeIPEndpoint NodeTable::getSourceEndpoint(bi::udp::endpoint const& from, PingNode const& packet) {
  if (from.address() != packet.source.address() && !isLocalHostAddress(packet.source.address())) {
    if (isPrivateAddress(from.address()) && !isPrivateAddress(packet.source.address())) {
      Guard l(x_ips);
      if (m_id2IpMap.contains(packet.sourceid)) {
        if (m_id2IpMap[packet.sourceid] != from) {
          m_ipMappings.erase(m_id2IpMap[packet.sourceid]);
          m_id2IpMap[packet.sourceid] = from;
        }
      } else {
        m_id2IpMap[packet.sourceid] = from;
      }
      m_ipMappings[from] = {packet.source.address(), packet.source.udpPort(), packet.source.tcpPort()};
      return m_ipMappings[from];
    }
  }
  return {from.address(), from.port(), packet.source.tcpPort()};
}

std::shared_ptr<NodeEntry> NodeTable::handlePingNode(bi::udp::endpoint const& _from, DiscoveryDatagram const& _packet) {
  auto const& in = dynamic_cast<PingNode const&>(_packet);

  if (in.version != dev::p2p::c_protocolVersion) {
    LOG(m_logger) << "Received a ping from a different protocol version node " << in.version << " from: " << _from;
    if (auto node = nodeEntry(_packet.sourceid)) dropNode(std::move(node));
    return nullptr;
  }

  if (in.chain_id != chain_id_) {
    LOG(m_logger) << "Received a ping from a different network node " << in.chain_id << " from: " << _from;
    if (auto node = nodeEntry(_packet.sourceid)) dropNode(std::move(node));
    return nullptr;
  }

  NodeIPEndpoint sourceEndpoint = getSourceEndpoint(_from, in);
  if (!addNode({in.sourceid, sourceEndpoint}))
    return {};  // Need to have valid endpoint proof before adding node to node
                // table.

  // Send PONG response.
  Pong p(sourceEndpoint);
  LOG(m_logger) << p.typeName() << " to " << in.sourceid << "@" << sourceEndpoint;
  p.expiration = nextRequestExpirationTime();
  p.echo = in.echo;
  p.seq = m_hostENR.sequenceNumber();
  p.sign(m_secret);
  m_socket->send(p);

  // Quirk: when the node is a replacement node (that is, not added to the node
  // table yet, but can be added after another node's eviction), it will not be
  // returned from nodeEntry() and we won't update its lastPongSentTime. But
  // that shouldn't be a big problem, at worst it can lead to more Ping-Pongs
  // than needed.
  std::shared_ptr<NodeEntry> sourceNodeEntry = nodeEntry(_packet.sourceid);
  if (sourceNodeEntry) {
    sourceNodeEntry->lastPongSentTime = RLPXDatagramFace::secondsSinceEpoch();
    // We should update entrypoint the one that node is reporting
    sourceNodeEntry->node.external_udp_port = in.source.udpPort();
  }

  return sourceNodeEntry;
}

std::shared_ptr<NodeEntry> NodeTable::handleENRRequest(bi::udp::endpoint const& _from,
                                                       DiscoveryDatagram const& _packet) {
  std::shared_ptr<NodeEntry> sourceNodeEntry = nodeEntry(_packet.sourceid);
  if (!sourceNodeEntry) {
    LOG(m_logger) << "Source node (" << _packet.sourceid << "@" << _from
                  << ") not found in node table. Ignoring ENRRequest request.";
    return {};
  }
  if (sourceNodeEntry->endpoint() != _from) {
    LOG(m_logger) << "ENRRequest packet from unexpected endpoint " << _from << " instead of "
                  << sourceNodeEntry->endpoint();
    return {};
  }
  if (!sourceNodeEntry->lastPongReceivedTime) {
    LOG(m_logger) << "Unexpected ENRRequest packet! Endpoint proof hasn't been "
                     "performed yet.";
    return {};
  }
  if (!sourceNodeEntry->hasValidEndpointProof()) {
    LOG(m_logger) << "Unexpected ENRRequest packet! Endpoint proof has expired.";
    return {};
  }

  auto const& in = dynamic_cast<ENRRequest const&>(_packet);

  ENRResponse response{_from, m_hostENR};
  LOG(m_logger) << response.typeName() << " to " << in.sourceid << "@" << _from;
  response.expiration = nextRequestExpirationTime();
  response.echo = in.echo;
  response.sign(m_secret);
  m_socket->send(response);

  return sourceNodeEntry;
}

std::shared_ptr<NodeEntry> NodeTable::handleENRResponse(bi::udp::endpoint const& _from,
                                                        DiscoveryDatagram const& _packet) {
  std::shared_ptr<NodeEntry> sourceNodeEntry = nodeEntry(_packet.sourceid);
  if (!sourceNodeEntry) {
    LOG(m_logger) << "Source node (" << _packet.sourceid << "@" << _from
                  << ") not found in node table. Ignoring ENRResponse packet.";
    return {};
  }
  if (sourceNodeEntry->endpoint() != _from) {
    LOG(m_logger) << "ENRResponse packet from unexpected endpoint " << _from << " instead of "
                  << sourceNodeEntry->endpoint();
    return {};
  }

  auto const& in = dynamic_cast<ENRResponse const&>(_packet);
  LOG(m_logger) << "Received ENR: " << *in.enr;

  return sourceNodeEntry;
}

void NodeTable::doDiscovery() {
  m_discoveryTimer->expires_after(c_bucketRefreshMs);
  auto discoveryTimer{m_discoveryTimer};
  m_discoveryTimer->async_wait(
      boost::asio::bind_executor(strand_, [this, discoveryTimer](boost::system::error_code const& _ec) {
        // We can't use m_logger if an error occurred because captured this
        // might be already destroyed
        if (_ec.value() == boost::asio::error::operation_aborted || discoveryTimer->expiry() == c_steadyClockMin) {
          clog(VerbosityDebug, "discov") << "Discovery timer was cancelled";
          return;
        } else if (_ec) {
          clog(VerbosityDebug, "discov") << "Discovery timer error detected: " << _ec.value() << " " << _ec.message();
          return;
        }

        NodeID randNodeId;
        crypto::Nonce::get().ref().copyTo(randNodeId.ref().cropped(0, static_cast<size_t>(h256::size)));
        crypto::Nonce::get().ref().copyTo(
            randNodeId.ref().cropped(static_cast<size_t>(h256::size), static_cast<size_t>(h256::size)));
        LOG(m_logger) << "Starting discovery algorithm run for random node id: " << randNodeId;
        doDiscoveryRound(randNodeId, 0 /* round */, std::make_shared<std::set<std::shared_ptr<NodeEntry>>>());
      }));
}

void NodeTable::doHandleTimeouts() {
  runBackgroundTask(c_handleTimeoutsIntervalMs, m_timeoutsTimer, [this]() {
    std::vector<std::shared_ptr<NodeEntry>> nodesToActivate;
    for (auto it = m_sentPings.begin(); it != m_sentPings.end();) {
      if (std::chrono::steady_clock::now() > it->second.pingSentTime + m_requestTimeToLive) {
        if (auto node = nodeEntry(it->second.nodeID)) {
          if (node->lastPongReceivedTime < RLPXDatagramFace::secondsSinceEpoch() - m_requestTimeToLive.count()) {
            if (it->first == node->endpoint()) {
              dropNode(std::move(node));
            } else {
              LOG(m_logger) << "Not dropping node " << it->second.nodeID << " on ping timeout for " << it->first << " "
                            << " as previous endpoint still valid " << node->endpoint();
            }
          } else {
            LOG(m_logger) << "Not dropping node " << it->second.nodeID << " " << it->first << " as it was updated";
          }
          // save the replacement node that should be activated
          if (it->second.replacementNodeEntry) nodesToActivate.emplace_back(std::move(it->second.replacementNodeEntry));
        }

        it = m_sentPings.erase(it);
      } else
        ++it;
    }

    // activate replacement nodes and put them into buckets
    for (auto const& n : nodesToActivate) noteActiveNode(n);
  });
}

void NodeTable::doEndpointTracking() {
  runBackgroundTask(c_removeOldEndpointStatementsIntervalMs, m_endpointTrackingTimer,
                    [this]() { m_endpointTracker.garbageCollectStatements(c_endpointStatementTimeToLiveMin); });
}

void NodeTable::runBackgroundTask(std::chrono::milliseconds const& _period, std::shared_ptr<ba::steady_timer> _timer,
                                  std::function<void()> _f) {
  _timer->expires_after(_period);
  _timer->async_wait(boost::asio::bind_executor(strand_, [=, this](boost::system::error_code const& _ec) {
    // We can't use m_logger if an error occurred because captured this
    // might be already destroyed
    if (_ec.value() == boost::asio::error::operation_aborted || _timer->expiry() == c_steadyClockMin) {
      clog(VerbosityDebug, "discov") << "Timer was cancelled";
      return;
    } else if (_ec) {
      clog(VerbosityDebug, "discov") << "Timer error detected: " << _ec.value() << " " << _ec.message();
      return;
    }

    _f();

    runBackgroundTask(_period, std::move(_timer), std::move(_f));
  }));
}

void NodeTable::cancelTimer(std::shared_ptr<ba::steady_timer> _timer) {
  // We "cancel" the timers by setting c_steadyClockMin rather than calling
  // cancel() because cancel won't set the boost error code if the timers have
  // already expired and the handlers are in the ready queue.
  //
  // Note that we "cancel" via io_context::post to ensure thread safety when
  // accessing the timers
  post(strand_, [_timer] { _timer->expires_at(c_steadyClockMin); });
}

std::unique_ptr<DiscoveryDatagram> DiscoveryDatagram::interpretUDP(bi::udp::endpoint const& _from,
                                                                   bytesConstRef _packet) {
  std::unique_ptr<DiscoveryDatagram> decoded;
  // h256 + Signature + type + RLP (smallest possible packet is empty neighbours
  // packet which is 3 bytes)
  if (_packet.size() < static_cast<size_t>(h256::size) + static_cast<size_t>(Signature::size) + 1 + 3) {
    LOG(g_discoveryWarnLogger::get()) << "Invalid packet (too small) from " << _from.address().to_string() << ":"
                                      << _from.port();
    return decoded;
  }
  bytesConstRef hashedBytes(
      _packet.cropped(static_cast<size_t>(h256::size), _packet.size() - static_cast<size_t>(h256::size)));
  bytesConstRef signedBytes(hashedBytes.cropped(static_cast<size_t>(Signature::size),
                                                hashedBytes.size() - static_cast<size_t>(Signature::size)));
  bytesConstRef signatureBytes(_packet.cropped(static_cast<size_t>(h256::size), static_cast<size_t>(Signature::size)));
  bytesConstRef bodyBytes(_packet.cropped(static_cast<size_t>(h256::size) + static_cast<size_t>(Signature::size) + 1));

  h256 echo(sha3(hashedBytes));
  if (!_packet.cropped(0, static_cast<size_t>(h256::size)).contentsEqual(echo.asBytes())) {
    LOG(g_discoveryWarnLogger::get()) << "Invalid packet (bad hash) from " << _from.address().to_string() << ":"
                                      << _from.port();
    return decoded;
  }
  Public sourceid(dev::recover(*(Signature const*)signatureBytes.data(), sha3(signedBytes)));
  if (!sourceid) {
    LOG(g_discoveryWarnLogger::get()) << "Invalid packet (bad signature) from " << _from.address().to_string() << ":"
                                      << _from.port();
    return decoded;
  }
  switch (signedBytes[0]) {
    case PingNode::type:
      decoded.reset(new PingNode(_from, sourceid, echo));
      break;
    case Pong::type:
      decoded.reset(new Pong(_from, sourceid, echo));
      break;
    case FindNode::type:
      decoded.reset(new FindNode(_from, sourceid, echo));
      break;
    case Neighbours::type:
      decoded.reset(new Neighbours(_from, sourceid, echo));
      break;
    case ENRRequest::type:
      decoded.reset(new ENRRequest(_from, sourceid, echo));
      break;
    case ENRResponse::type:
      decoded.reset(new ENRResponse(_from, sourceid, echo));
      break;
    default:
      LOG(g_discoveryWarnLogger::get()) << "Invalid packet (unknown packet type) from " << _from.address().to_string()
                                        << ":" << _from.port();
      return decoded;
  }
  decoded->interpretRLP(bodyBytes);
  return decoded;
}

}  // namespace p2p
}  // namespace dev
