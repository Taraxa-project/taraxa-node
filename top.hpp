#ifndef TARAXA_NODE_TOP_HPP
#define TARAXA_NODE_TOP_HPP

#include <libdevcore/Log.h>
#include <libdevcore/LoggingProgramOptions.h>
#include <libweb3jsonrpc/EthFace.h>
#include <libweb3jsonrpc/ModularServer.h>

#include <atomic>
#include <boost/process.hpp>

#include "full_node.hpp"
#include "net/Net.h"
#include "net/Taraxa.h"
#include "net/Test.h"
#include "net/WSServer.h"

class Top {
  boost::asio::io_context rpc_io_context_;
  taraxa::FullNode::container node_;
  std::thread rpc_thread_;

 public:
  Top(int argc, const char* argv[]);
  ~Top();

  // may return null which means there was an error in constructor
  decltype(node_)::shared_t getNode() { return node_; }
  void join();
};

#endif
