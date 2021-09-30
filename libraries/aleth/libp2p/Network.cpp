// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include <sys/types.h>
#ifndef _WIN32
#include <ifaddrs.h>
#endif

#include <libdevcore/Assertions.h>
#include <libdevcore/Common.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/Exceptions.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "Common.h"
#include "Network.h"
#include "UPnP.h"

using namespace std;
using namespace dev;
using namespace dev::p2p;

static_assert(BOOST_VERSION >= 106400, "Wrong boost headers version");

std::set<bi::address> Network::getInterfaceAddresses() {
  std::set<bi::address> addresses;

#if defined(_WIN32)
  WSAData wsaData;
  if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) BOOST_THROW_EXCEPTION(NoNetworking());

  char ac[80];
  if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
    cnetlog << "Error " << WSAGetLastError() << " when getting local host name.";
    WSACleanup();
    BOOST_THROW_EXCEPTION(NoNetworking());
  }

  struct hostent* phe = gethostbyname(ac);
  if (phe == 0) {
    cnetlog << "Bad host lookup.";
    WSACleanup();
    BOOST_THROW_EXCEPTION(NoNetworking());
  }

  for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
    struct in_addr addr;
    memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
    char* addrStr = inet_ntoa(addr);
    bi::address address(bi::make_address(addrStr));
    if (!isLocalHostAddress(address)) addresses.insert(address.to_v4());
  }

  WSACleanup();
#else
  ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) BOOST_THROW_EXCEPTION(NoNetworking());

  for (auto ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || string(ifa->ifa_name) == "lo0" || !(ifa->ifa_flags & IFF_UP)) continue;

    if (ifa->ifa_addr->sa_family == AF_INET) {
      in_addr addr = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
      boost::asio::ip::address_v4 address(boost::asio::detail::socket_ops::network_to_host_long(addr.s_addr));
      if (!isLocalHostAddress(address)) addresses.insert(address);
    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
      sockaddr_in6* sockaddr = ((struct sockaddr_in6*)ifa->ifa_addr);
      in6_addr addr = sockaddr->sin6_addr;
      boost::asio::ip::address_v6::bytes_type bytes;
      memcpy(&bytes[0], addr.s6_addr, 16);
      boost::asio::ip::address_v6 address(bytes, sockaddr->sin6_scope_id);
      if (!isLocalHostAddress(address)) addresses.insert(address);
    }
  }

  if (ifaddr != NULL) freeifaddrs(ifaddr);

#endif

  return addresses;
}

Network::PortBindException::PortBindException(bi::address bind_addr, unsigned port, std::exception const& cause)
    : std::runtime_error(
          "Couldn't start accepting connections on host. Failed to accept "
          "socket on " +
          bind_addr.to_string() + ":" + std::to_string(port) + ".\n" + boost::diagnostic_information(cause)),
      bind_addr(std::move(bind_addr)),
      port(port),
      cause(cause) {}

void Network::tcp4Listen(bi::tcp::acceptor& _acceptor, NetworkConfig const& _config) {
  // Due to the complexities of NAT and network environments (multiple NICs,
  // tunnels, etc) and security concerns automation is the enemy of network
  // configuration. If a preference cannot be accommodate the network must fail
  // to start.
  //
  // Preferred IP: Attempt if set, else, try 0.0.0.0 (all interfaces)
  // Preferred Port: Attempt if set, else, try c_defaultListenPort or 0 (random)

  auto listenIP = _config.listenIPAddress.empty() ? bi::address_v4() : bi::make_address(_config.listenIPAddress);
  bi::tcp::endpoint endpoint(listenIP, _config.listenPort);
  try {
#if defined(_WIN32)
    bool reuse = false;
#else
    bool reuse = true;
#endif
    _acceptor.open(endpoint.protocol());
    _acceptor.set_option(ba::socket_base::reuse_address(reuse));
    _acceptor.bind(endpoint);
    _acceptor.listen();
  } catch (std::exception const& e) {
    _acceptor.close();
    throw PortBindException(listenIP, _config.listenPort, e);
  }
}

bi::tcp::endpoint Network::traverseNAT(std::set<bi::address> const& _ifAddresses, unsigned short _listenPort,
                                       bi::address& o_upnpInterfaceAddr) {
  asserts(_listenPort != 0);

  unique_ptr<UPnP> upnp;
  try {
    upnp.reset(new UPnP);
  }
  // let m_upnp continue as null - we handle it properly.
  catch (...) {
  }

  bi::tcp::endpoint upnpEP;
  if (upnp && upnp->isValid()) {
    bi::address pAddr;
    int extPort = 0;
    for (auto const& addr : _ifAddresses)
      if (addr.is_v4() && isPrivateAddress(addr) &&
          (extPort = upnp->addRedirect(addr.to_string().c_str(), _listenPort))) {
        pAddr = addr;
        break;
      }

    auto eIP = upnp->externalIP();
    bi::address eIPAddr(bi::make_address(eIP));
    if (extPort && eIP != string("0.0.0.0") && !isPrivateAddress(eIPAddr)) {
      cnetnote << "Punched through NAT and mapped local port " << _listenPort << " onto external port " << extPort
               << ".";
      cnetnote << "External addr: " << eIP;
      o_upnpInterfaceAddr = pAddr;
      upnpEP = bi::tcp::endpoint(eIPAddr, (unsigned short)extPort);
    } else
      cnetlog << "Couldn't punch through NAT (or no NAT in place).";
  }

  return upnpEP;
}

bi::tcp::endpoint Network::resolveHost(string const& _addr) {
  static boost::asio::io_context s_resolverIoContext;

  vector<string> split;
  boost::split(split, _addr, boost::is_any_of(":"));
  unsigned port = dev::p2p::c_defaultListenPort;

  try {
    if (split.size() > 1) port = static_cast<unsigned>(stoi(split.at(1)));
  } catch (...) {
  }

  boost::system::error_code ec;
  bi::address address = bi::make_address(split[0], ec);
  bi::tcp::endpoint ep(bi::address(), port);
  if (!ec)
    ep.address(address);
  else {
    boost::system::error_code ec;
    // resolve returns an iterator (host can resolve to multiple addresses)
    bi::tcp::resolver r(s_resolverIoContext);
    auto res = r.resolve(bi::tcp::v4(), split[0], toString(port), ec);
    if (ec || res.empty()) {
      cnetlog << "Error resolving host address... " << _addr << " : " << ec.message();
      return bi::tcp::endpoint();
    } else
      ep = *res;
  }
  return ep;
}
