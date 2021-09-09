// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "Host.h"

#include <libdevcore/Assertions.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Exceptions.h>
#include <libdevcore/FileSystem.h>

#include <boost/algorithm/string.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "Capability.h"
#include "Common.h"
#include "RLPxHandshake.h"
#include "Session.h"
#include "UPnP.h"

using namespace std;
using namespace dev;
using namespace dev::p2p;

Host::Host(std::string _clientVersion, KeyPair const& kp, NetworkConfig _n, TaraxaNetworkConfig taraxa_conf,
           std::filesystem::path state_file_path)
    : ioc_(taraxa_conf.expected_parallelism),
      ioc_w_(ba::make_work_guard(ioc_)),
      session_ioc_(taraxa_conf.expected_parallelism),
      session_ioc_w_(ba::make_work_guard(session_ioc_)),
      strand_(ioc_),
      m_tcp4Acceptor(session_ioc_),
      m_runTimer(ioc_),
      state_file_path_(move(state_file_path)),
      m_clientVersion(move(_clientVersion)),
      m_netConfig(move(_n)),
      taraxa_conf_(taraxa_conf),
      m_idealPeerCount(taraxa_conf_.ideal_peer_count),
      m_stretchPeers(taraxa_conf_.peer_stretch),
      m_listenPort(m_netConfig.listenPort),
      m_alias{kp},
      m_lastPing(chrono::steady_clock::time_point::min()),
      m_lastPeerLogMessage(chrono::steady_clock::time_point::min()) {
  assert(m_netConfig.listenPort);
  assert(1 <= taraxa_conf.expected_parallelism);
  // try to open acceptor (todo: ipv6)
  Network::tcp4Listen(m_tcp4Acceptor, m_netConfig);
  m_tcpPublic = determinePublic();
  RLPXHandshake::HostContext handshake_ctx{m_alias, {}, {}, {}, {}, {}};
  handshake_ctx.port = m_listenPort;
  handshake_ctx.client_version = m_clientVersion;
  handshake_ctx.on_success = [this](auto const& id, auto const& rlp, auto frame_coder, auto socket) {
    ba::post(strand_, [=, this, _ = shared_from_this(), rlp = rlp.data().cropped(0, rlp.actualSize()).toBytes(),
                       frame_coder = move(frame_coder)]() mutable {
      startPeerSession(id, RLP(rlp), move(frame_coder), socket);
    });
  };
  handshake_ctx.on_failure = [this](auto const& id, auto reason) {
    ba::post(strand_, [=, this] {
      if (auto p = peer(id)) {
        p->m_lastHandshakeFailure = reason;
      }
    });
  };
  handshake_ctx_ = make_shared<decltype(handshake_ctx)>(move(handshake_ctx));
  auto restored_state = restore_state();
  auto const& enr = restored_state ? restored_state->enr
                                   : IdentitySchemeV4::createENR(m_alias.secret(),
                                                                 m_netConfig.publicIPAddress.empty()
                                                                     ? bi::address{}
                                                                     : bi::make_address(m_netConfig.publicIPAddress),
                                                                 m_netConfig.listenPort, m_netConfig.listenPort);
  if (restored_state) {
    for (auto const& peer : restored_state->peers) {
      m_peers[peer->id] = peer;
    }
  }
  m_nodeTable = make_unique<NodeTable>(ioc_, m_alias,
                                       NodeIPEndpoint(bi::make_address(listenAddress()), listenPort(), listenPort()),
                                       updateENR(enr, m_tcpPublic, listenPort()), m_netConfig.discovery,
                                       m_netConfig.allowLocalDiscovery, taraxa_conf_.is_boot_node);
  m_nodeTable->setEventHandler(new NodeTableEventHandler([this](auto const&... args) { onNodeTableEvent(args...); }));
  if (restored_state) {
    for (auto const& node : restored_state->known_nodes) {
      addKnownNodeToNodeTable(node);
    }
    for (auto const& peer : restored_state->peers) {
      m_nodeTable->addNode(*peer);
    }
  }
  LOG(m_logger) << "devp2p started. Node id: " << id();
  runAcceptor();
  ba::post(strand_, [this] { main_loop_body(); });
}

std::shared_ptr<Host> Host::make(std::string _clientVersion, CapabilitiesFactory const& cap_factory, KeyPair const& kp,
                                 NetworkConfig _n, TaraxaNetworkConfig taraxa_conf,
                                 std::filesystem::path state_file_path) {
  shared_ptr<Host> self(new Host(move(_clientVersion), kp, move(_n), taraxa_conf, move(state_file_path)));
  for (auto const& cap : cap_factory(self)) {
    CapabilityNameAndVersion cap_id{cap->name(), cap->version()};
    self->m_capabilities.emplace(cap_id, Capability(cap, cap->messageCount()));
    self->handshake_ctx_->capability_descriptions.push_back(cap_id);
  }
  self->fully_initialized_ = true;
  return self;
}

Host::~Host() {
  // reset io_context (allows manually polling network, below)
  ioc_.stop();
  session_ioc_.restart();

  // shutdown acceptor
  m_tcp4Acceptor.cancel();
  if (m_tcp4Acceptor.is_open()) m_tcp4Acceptor.close();

  for (auto& wp : m_connecting) {
    if (auto handshake = wp.lock()) {
      handshake->cancel();
    }
  }
  for (auto const& i : m_sessions) {
    if (auto s = i.second.lock()) {
      s->disconnect(ClientQuit);
    }
  }
  while (0 < session_ioc_.poll())
    ;
  save_state();
}

ba::io_context::count_type Host::do_work() {
  ba::io_context::count_type ret = 0;
  if (fully_initialized_) {
    try {
      ret += ioc_.poll();
      ret += session_ioc_.poll();
    } catch (std::exception const& e) {
      cerror << "Host::do_work exception: " << e.what();
    }
  }
  return ret;
}

void Host::addNode(Node const& _node) {
  if (_node.id == id()) {
    cnetdetails << "Ignoring the request to connect to self " << _node;
    return;
  }
  if (_node.peerType == PeerType::Optional) {
    m_nodeTable->addNode(_node);
    return;
  }
  assert(_node.peerType == PeerType::Required);
  ba::post(strand_, [=, this] {
    // create or update m_peers entry
    shared_ptr<Peer> p;
    auto it = m_peers.find(_node.id);
    if (it != m_peers.end()) {
      p = it->second;
      p->set_endpoint(_node.get_endpoint());
      p->peerType = PeerType::Required;
    } else {
      p = make_shared<Peer>(_node);
      m_peers[_node.id] = p;
    }
    m_nodeTable->addNode(*p);
  });
}

std::shared_ptr<Peer> Host::peer(NodeID const& _n) const {
  auto it = m_peers.find(_n);
  return it != m_peers.end() ? it->second : nullptr;
}

// Starts a new peer session after a successful handshake - agree on
// mutually-supported capablities, start each mutually-supported capability, and
// send a ping to the node.
void Host::startPeerSession(Public const& _id, RLP const& _hello, unique_ptr<RLPXFrameCoder> _io,
                            shared_ptr<RLPXSocket> const& _s) {
  // session maybe ingress or egress so m_peers and node table entries may not
  // exist
  shared_ptr<Peer> peer;
  auto itPeer = m_peers.find(_id);
  if (itPeer != m_peers.end()) {
    peer = itPeer->second;
    peer->m_lastHandshakeFailure = HandshakeFailureReason::NoFailure;
  } else {
    // peer doesn't exist, try to get port info from node table
    if (auto n = nodeFromNodeTable(_id)) {
      peer = make_shared<Peer>(n);
    }
    if (!peer) {
      peer = make_shared<Peer>(Node(_id, UnspecifiedNodeIPEndpoint));
    }
    m_peers[_id] = peer;
  }
  peer->m_lastConnected = chrono::system_clock::now();
  {
    auto endpoint = peer->get_endpoint();
    endpoint.setAddress(_s->remoteEndpoint().address());
    peer->set_endpoint(move(endpoint));
  }
  auto const protocolVersion = _hello[0].toInt<unsigned>();
  auto const clientVersion = _hello[1].toString();
  auto caps = _hello[2].toVector<CapDesc>();
  auto const listenPort = _hello[3].toInt<unsigned short>();
  auto const pub = _hello[4].toHash<Public>();

  if (pub != _id) {
    cdebug << "Wrong ID: " << pub << " vs. " << _id;
    return;
  }

  // leave only highset mutually supported capability version
  caps.erase(remove_if(caps.begin(), caps.end(),
                       [&](CapDesc const& _r) {
                         return !haveCapability(_r) || any_of(caps.begin(), caps.end(), [&](CapDesc const& _o) {
                           return _r.first == _o.first && _o.second > _r.second && haveCapability(_o);
                         });
                       }),
             caps.end());

  // clang error (previously: ... << hex << caps ...)
  // "'operator<<' should be declared prior to the call site or in an associated
  // namespace of one of its arguments"
  stringstream capslog;
  for (auto const& cap : caps) {
    capslog << "(" << cap.first << "," << dec << cap.second << ")";
  }

  cnetlog << "Starting peer session with " << clientVersion << " (protocol: V" << protocolVersion << ") " << _id << " "
          << showbase << "capabilities: " << capslog.str() << " " << dec << "port: " << listenPort;

  std::optional<DisconnectReason> disconnect_reason;
  SessionCapabilities session_caps;
  auto cap_offset = (unsigned)UserPacket;
  for (auto const& capDesc : caps) {
    auto itCap = m_capabilities.find(capDesc);
    if (itCap == m_capabilities.end()) {
      disconnect_reason = IncompatibleProtocol;
      break;
    }
    session_caps.emplace(capDesc.first, SessionCapability(itCap->second, capDesc.second, cap_offset));
    cnetlog << "New session for capability " << capDesc.first << "; idOffset: " << cap_offset << " with " << _id << "@"
            << _s->remoteEndpoint();
    cap_offset += itCap->second.message_count;
  }
  if (!disconnect_reason && protocolVersion < dev::p2p::c_protocolVersion - 1) {
    disconnect_reason = IncompatibleProtocol;
  }
  if (!disconnect_reason && caps.empty()) {
    disconnect_reason = UselessPeer;
  }
  if (!disconnect_reason && m_netConfig.pin && peer->peerType != PeerType::Required) {
    cdebug << "Unexpected identity from peer (peer is not marked required)";
    disconnect_reason = UnexpectedIdentity;
  }
  if (!disconnect_reason && m_sessions.count(_id)) {
    if (auto s = m_sessions[_id].lock()) {
      if (s->isConnected()) {
        // Already connected.
        cnetlog << "Session already exists for peer with id " << _id;
        disconnect_reason = DuplicatePeer;
      }
    }
  }
  if (!disconnect_reason && !peerSlotsAvailable()) {
    cnetdetails << "Too many peers, can't connect. peer count: " << peer_count_()
                << " pending peers: " << m_pendingPeerConns.size();
    disconnect_reason = TooManyPeers;
  }
  auto session = Session::make(session_caps, move(_io), _s, peer,
                               PeerSessionInfo{
                                   _id,
                                   clientVersion,
                                   peer->get_endpoint().address().to_string(),
                                   listenPort,
                                   chrono::steady_clock::duration(),
                                   _hello[2].toSet<CapDesc>(),
                               },
                               disconnect_reason);
  if (!disconnect_reason) {
    m_sessions[_id] = session;
    LOG(m_logger) << "Peer connection successfully established with " << _id << "@" << _s->remoteEndpoint();
  }
}

/// Get session by id
shared_ptr<Session> Host::peerSession(NodeID const& _id) const {
  auto const it = m_sessions.find(_id);
  if (it != m_sessions.end()) {
    auto s = it->second.lock();
    if (s && s->isConnected()) {
      return s;
    }
  }
  return {};
}

void Host::onNodeTableEvent(NodeID const& _n, NodeTableEventType const& _e) {
  if (_e == NodeEntryAdded) {
    LOG(m_logger) << "p2p.host.nodeTable.events.nodeEntryAdded " << _n;
    if (Node n = nodeFromNodeTable(_n)) {
      shared_ptr<Peer> p;
      if (m_peers.count(_n)) {
        p = m_peers[_n];
        p->set_endpoint(n.get_endpoint());
      } else {
        p = make_shared<Peer>(n);
        m_peers[_n] = p;
        LOG(m_logger) << "p2p.host.peers.events.peerAdded " << _n << " " << p->get_endpoint();
      }
      if (peerSlotsAvailable(Egress)) connect(p);
    }
  } else if (_e == NodeEntryDropped) {
    LOG(m_logger) << "p2p.host.nodeTable.events.NodeEntryDropped " << _n;
    if (m_peers.count(_n) && m_peers[_n]->peerType == PeerType::Optional) m_peers.erase(_n);
  }
}

bool Host::isHandshaking(NodeID const& _id) const {
  for (auto const& cIter : m_connecting) {
    std::shared_ptr<RLPXHandshake> const connecting = cIter.lock();
    if (connecting && connecting->remote() == _id) return true;
  }
  return false;
}

bi::tcp::endpoint Host::determinePublic() const {
  // return listenIP (if public) > public > upnp > unspecified address.

  auto ifAddresses = Network::getInterfaceAddresses();
  auto laddr = m_netConfig.listenIPAddress.empty() ? bi::address() : bi::make_address(m_netConfig.listenIPAddress);
  auto lset = !laddr.is_unspecified();
  auto paddr = m_netConfig.publicIPAddress.empty() ? bi::address() : bi::make_address(m_netConfig.publicIPAddress);
  auto pset = !paddr.is_unspecified();

  bool listenIsPublic = lset && isPublicAddress(laddr);
  bool publicIsHost = !lset && pset && ifAddresses.count(paddr);

  bi::tcp::endpoint ep(bi::address(), m_listenPort);
  if (m_netConfig.traverseNAT && listenIsPublic) {
    cnetnote << "Listen address set to Public address: " << laddr << ". UPnP disabled.";
    ep.address(laddr);
  } else if (m_netConfig.traverseNAT && publicIsHost) {
    cnetnote << "Public address set to Host configured address: " << paddr << ". UPnP disabled.";
    ep.address(paddr);
  } else if (m_netConfig.traverseNAT) {
    bi::address natIFAddr;
    ep = Network::traverseNAT(lset && ifAddresses.count(laddr) ? set<bi::address>({laddr}) : ifAddresses, m_listenPort,
                              natIFAddr);

    if (lset && natIFAddr != laddr)
      // if listen address is set, Host will use it, even if upnp returns
      // different
      cwarn << "Listen address " << laddr << " differs from local address " << natIFAddr << " returned by UPnP!";

    if (pset && ep.address() != paddr) {
      // if public address is set, Host will advertise it, even if upnp returns
      // different
      cwarn << "Specified public address " << paddr << " differs from external address " << ep.address()
            << " returned by UPnP!";
      ep.address(paddr);
    }
  } else if (pset)
    ep.address(paddr);

  return ep;
}

ENR Host::updateENR(ENR const& _restoredENR, bi::tcp::endpoint const& _tcpPublic, uint16_t const& _listenPort) {
  auto const address = _tcpPublic.address().is_unspecified() ? _restoredENR.ip() : _tcpPublic.address();

  if (_restoredENR.ip() == address && _restoredENR.tcpPort() == _listenPort && _restoredENR.udpPort() == _listenPort)
    return _restoredENR;

  auto newENR = IdentitySchemeV4::updateENR(_restoredENR, m_alias.secret(), address, _listenPort, _listenPort);
  LOG(m_infoLogger) << "ENR updated: " << newENR;

  return newENR;
}

void Host::runAcceptor() {
  m_tcp4Acceptor.async_accept(make_strand(session_ioc_),
                              [=, this](boost::system::error_code _ec, bi::tcp::socket _socket) {
                                if (_ec == ba::error::operation_aborted || !m_tcp4Acceptor.is_open()) {
                                  return;
                                }
                                auto socket = make_shared<RLPXSocket>(std::move(_socket));
                                if (peer_count_() > peerSlots(Ingress)) {
                                  cnetdetails << "Dropping incoming connect due to maximum peer count (" << Ingress
                                              << " * ideal peer count): " << socket->remoteEndpoint();
                                  socket->close();
                                } else {
                                  // incoming connection; we don't yet know nodeid
                                  auto handshake = make_shared<RLPXHandshake>(handshake_ctx_, socket);
                                  ba::post(strand_, [=, this, this_shared = shared_from_this()] {
                                    m_connecting.push_back(handshake);
                                    handshake->start();
                                  });
                                }
                                runAcceptor();
                              });
}

void Host::invalidateNode(NodeID const& _node) { m_nodeTable->invalidateNode(_node); }

void Host::connect(shared_ptr<Peer> const& _p) {
  if (isHandshaking(_p->id)) {
    cwarn << "Aborted connection. RLPX handshake with peer already in progress: " << _p->id << "@"
          << _p->get_endpoint();
    return;
  }

  if (havePeerSession(_p->id)) {
    cnetdetails << "Aborted connection. Peer already connected: " << _p->id << "@" << _p->get_endpoint();
    return;
  }

  if (!nodeTableHasNode(_p->id) && _p->peerType == PeerType::Optional) {
    return;
  }

  // prevent concurrently connecting to a node
  Peer* nptr = _p.get();
  if (m_pendingPeerConns.count(nptr)) {
    return;
  }
  m_pendingPeerConns.insert(nptr);

  _p->m_lastAttempted = chrono::system_clock::now();

  bi::tcp::endpoint ep(_p->get_endpoint());
  cnetdetails << "Attempting connection to " << _p->id << "@" << ep << " from " << id();
  auto socket = make_shared<RLPXSocket>(bi::tcp::socket(make_strand(session_ioc_)));
  socket->ref().async_connect(
      ep, ba::bind_executor(strand_, [=, this, this_shared = shared_from_this()](boost::system::error_code const& ec) {
        _p->m_lastAttempted = chrono::system_clock::now();
        _p->m_failedAttempts++;

        if (ec) {
          cnetdetails << "Connection refused to node " << _p->id << "@" << ep << " (" << ec.message() << ")";
          // Manually set error (session not present)
          _p->m_lastDisconnect = TCPError;
        } else {
          cnetdetails << "Starting RLPX handshake with " << _p->id << "@" << ep;
          auto handshake = make_shared<RLPXHandshake>(handshake_ctx_, socket, _p->id);
          m_connecting.push_back(handshake);

          handshake->start();
        }
        m_pendingPeerConns.erase(nptr);
      }));
}

PeerSessionInfos Host::peerSessionInfos() const {
  vector<PeerSessionInfo> ret;
  for (auto& i : m_sessions)
    if (auto j = i.second.lock())
      if (j->isConnected()) ret.push_back(j->info());
  return ret;
}

size_t Host::peer_count_() const {
  unsigned retCount = 0;
  for (auto& i : m_sessions)
    if (shared_ptr<Session> j = i.second.lock())
      if (j->isConnected()) retCount++;
  return retCount;
}

void Host::main_loop_body() {
  // This again requires x_nodeTable, which is why an additional variable
  // nodeTable is used.
  m_nodeTable->processEvents();

  // cleanup zombies
  m_connecting.remove_if([](auto const& h) { return h.expired(); });

  keepAlivePeers();
  logActivePeers();

  // At this time peers will be disconnected based on natural TCP timeout.
  // disconnectLatePeers needs to be updated for the assumption that Session
  // is always live and to ensure reputation and fallback timers are properly
  // updated. // disconnectLatePeers();

  // todo: update peerSlotsAvailable()

  list<shared_ptr<Peer>> toConnect;
  unsigned reqConn = 0;
  {
    auto p = m_peers.cbegin();
    while (p != m_peers.cend()) {
      bool peerRemoved = false;
      bool haveSession = havePeerSession(p->second->id);
      bool required = p->second->peerType == PeerType::Required;
      if (haveSession && required)
        reqConn++;
      else if (!haveSession) {
        if (p->second->isUseless()) {
          peerRemoved = true;
          p = m_peers.erase(p);
        } else if (p->second->shouldReconnect() && (!m_netConfig.pin || required))
          toConnect.push_back(p->second);
      }
      if (!peerRemoved) p++;
    }
  }

  for (auto const& p : toConnect) {
    if (p->peerType == PeerType::Required && reqConn++ < m_idealPeerCount) {
      connect(p);
    }
  }

  if (!m_netConfig.pin) {
    unsigned const maxSlots = m_idealPeerCount + reqConn;
    unsigned occupiedSlots = peer_count_() + m_pendingPeerConns.size();
    for (auto peerToConnect = toConnect.cbegin(); occupiedSlots <= maxSlots && peerToConnect != toConnect.cend();
         ++peerToConnect) {
      if ((*peerToConnect)->peerType == PeerType::Optional) {
        connect(*peerToConnect);
        ++occupiedSlots;
      }
    }
  }

  peer_count_snapshot_ = peer_count_();

  m_runTimer.expires_after(taraxa_conf_.main_loop_interval);
  m_runTimer.async_wait([this](...) { main_loop_body(); });
}

void Host::keepAlivePeers() {
  if (chrono::steady_clock::now() - taraxa_conf_.peer_healthcheck_interval < m_lastPing) {
    return;
  }
  for (auto it = m_sessions.begin(); it != m_sessions.end();) {
    auto p = it->second.lock();
    if (p && p->isConnected()) {
      p->ping();
      ++it;
    } else {
      it = m_sessions.erase(it);
    }
  }
  m_lastPing = chrono::steady_clock::now();
}

void Host::logActivePeers() {
  if (chrono::steady_clock::now() - taraxa_conf_.log_active_peers_interval < m_lastPeerLogMessage) {
    return;
  }

  LOG(m_infoLogger) << "Active peer count: " << peer_count_();
  if (m_netConfig.discovery) LOG(m_infoLogger) << "Looking for peers...";

  LOG(m_logger) << "Peers: " << peerSessionInfos();
  m_lastPeerLogMessage = chrono::steady_clock::now();
}

void Host::save_state() const {
  if (state_file_path_.empty()) {
    return;
  }
  RLPStream network;
  auto nodeTableEntries = m_nodeTable->snapshot();
  int count = 0;
  for (auto const& entry : nodeTableEntries) {
    network.appendList(6);
    entry.endpoint().streamRLP(network, NodeIPEndpoint::StreamInline);
    network << entry.id() << entry.lastPongReceivedTime << entry.lastPongSentTime;
    count++;
  }

  vector<Peer> peers;
  for (auto const& p : m_peers)
    if (p.second) peers.push_back(*p.second);

  for (auto const& p : peers) {
    // todo: ipv6
    auto endpoint = p.get_endpoint();
    if (!endpoint.address().is_v4()) continue;

    // Only save peers which have connected within 2 days, with
    // properly-advertised port and public IP address
    if (chrono::system_clock::now() - p.m_lastConnected.load() < chrono::seconds(3600 * 48) && endpoint &&
        p.id != id() && (p.peerType == PeerType::Required || isAllowedEndpoint(endpoint))) {
      network.appendList(9);
      endpoint.streamRLP(network, NodeIPEndpoint::StreamInline);
      network << p.id << (p.peerType == PeerType::Required)
              << chrono::duration_cast<chrono::seconds>(p.m_lastConnected.load().time_since_epoch()).count()
              << chrono::duration_cast<chrono::seconds>(p.m_lastAttempted.load().time_since_epoch()).count()
              << p.m_failedAttempts.load() << (unsigned)p.m_lastDisconnect;
      count++;
    }
  }

  RLPStream ret(3);
  ret << dev::p2p::c_protocolVersion;

  ret.appendList(2);
  ret << m_alias.secret().ref();
  enr().streamRLP(ret);

  ret.appendList(count);
  if (count) {
    ret.appendRaw(network.out(), count);
  }
  ofstream out(state_file_path_, ios::binary | ios::out | ios::trunc);
  for (auto b : ret.out()) {
    out << b;
  }
}

std::optional<Host::PersistentState> Host::restore_state() {
  if (state_file_path_.empty() || !filesystem::exists(state_file_path_)) {
    return nullopt;
  }
  assert(filesystem::is_regular_file(state_file_path_));
  ifstream state_file_stream(state_file_path_);
  string state_file_contents{istreambuf_iterator(state_file_stream), {}};
  if (state_file_contents.empty()) {
    return nullopt;
  }
  auto [restored_secret, restored_enr] = restoreENR(state_file_contents, m_netConfig);
  assert(restored_secret == m_alias.secret());
  PersistentState ret{move(restored_enr), {}, {}};
  RLP r(state_file_contents);
  auto const protocolVersion = r[0].toInt<unsigned>();
  if (r.itemCount() > 0 && r[0].isInt() && protocolVersion >= dev::p2p::c_protocolVersion) {
    // r[0] = version
    // r[1] = key
    // r[2] = nodes

    for (auto const nodeRLP : r[2]) {
      // nodeRLP[0] - IP address
      // todo: ipv6
      if (nodeRLP[0].itemCount() != 4 && nodeRLP[0].size() != 4) continue;

      Node node((NodeID)nodeRLP[3], NodeIPEndpoint(nodeRLP));
      if (nodeRLP.itemCount() == 6 && isAllowedEndpoint(node.get_endpoint())) {
        // node was saved from the node table
        auto const lastPongReceivedTime = nodeRLP[4].toInt<uint32_t>();
        auto const lastPongSentTime = nodeRLP[5].toInt<uint32_t>();
        ret.known_nodes.push_back({
            node,
            lastPongReceivedTime,
            lastPongSentTime,
        });
      } else if (nodeRLP.itemCount() == 9) {
        // node was saved from the connected peer list
        node.peerType = nodeRLP[4].toInt<bool>() ? PeerType::Required : PeerType::Optional;
        if (!isAllowedEndpoint(node.get_endpoint()) && node.peerType == PeerType::Optional) continue;
        auto peer = make_shared<Peer>(node);
        peer->m_lastConnected = chrono::system_clock::time_point(chrono::seconds(nodeRLP[5].toInt<unsigned>()));
        peer->m_lastAttempted = chrono::system_clock::time_point(chrono::seconds(nodeRLP[6].toInt<unsigned>()));
        peer->m_failedAttempts = nodeRLP[7].toInt<unsigned>();
        peer->m_lastDisconnect = (DisconnectReason)nodeRLP[8].toInt<unsigned>();
        ret.peers.emplace_back(move(peer));
      }
    }
  }
  return ret;
}

bool Host::peerSlotsAvailable(Host::PeerSlotType _type /*= Ingress*/) {
  return peer_count_() + m_pendingPeerConns.size() < peerSlots(_type);
}

std::pair<Secret, ENR> Host::restoreENR(bytesConstRef _b, NetworkConfig const& _netConfig) {
  RLP r(_b);
  Secret secret;
  if (r.itemCount() == 3 && r[0].isInt() && r[0].toInt<unsigned>() >= 3) {
    if (r[1].isList()) {
      secret = Secret{r[1][0].toBytes()};
      auto enrRlp = r[1][1];

      return make_pair(secret, IdentitySchemeV4::parseENR(enrRlp));
    }

    // Support for older format without ENR
    secret = Secret{r[1].toBytes()};
  } else {
    // no private key found, create new one
    secret = KeyPair::create().secret();
  }

  auto const address =
      _netConfig.publicIPAddress.empty() ? bi::address{} : bi::make_address(_netConfig.publicIPAddress);

  return make_pair(secret, IdentitySchemeV4::createENR(secret, address, _netConfig.listenPort, _netConfig.listenPort));
}

bool Host::nodeTableHasNode(Public const& _id) const { return m_nodeTable->haveNode(_id); }

Node Host::nodeFromNodeTable(Public const& _id) const { return m_nodeTable->node(_id); }

bool Host::addKnownNodeToNodeTable(KnownNode const& node) {
  if (node.node.id == id()) {
    cnetdetails << "Ignoring the request to connect to self " << node.node;
    return false;
  }
  return m_nodeTable->addKnownNode(node.node, node.lastPongReceivedTime, node.lastPongSentTime);
}
