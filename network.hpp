/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-12-11 16:03:02
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:13:00
 */

#ifndef NETWORK_HPP
#define NETWORK_HPP
#include "full_node.hpp"
#include "libp2p/Host.h"
#include "dag_block.hpp"
#include "taraxa_capability.h"
#include "util.hpp"
#include <boost/thread.hpp>
#include <condition_variable>
#include <iostream>
#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Capability.h>
#include <libp2p/CapabilityHost.h>
#include <libp2p/Common.h>
#include <libp2p/Network.h>
#include <libp2p/Session.h>
#include <mutex>
#include <string>

namespace taraxa {

class FullNode;

struct NodeConfig {
	std::string id;
	std::string ip;
	uint16_t port;
};

struct NetworkConfig {
	NetworkConfig(std::string const &json_file);
	std::string json_file_name;
	uint16_t network_listen_port;
	std::string network_node_id;
	std::vector<NodeConfig> network_boot_nodes;
};

/**
 */

class Network {
public:
	Network(std::string const &conf_file_name);
	Network(std::string const &conf_file_name, std::string networkFile);
	~Network();
	void start();
	void stop();
	void rpcAction(boost::system::error_code const &ec, size_t size);
	void sendTest(dev::p2p::NodeID const &id);
	void sendBlock(dev::p2p::NodeID const &id, DagBlock const &blk);
	void onNewBlock(DagBlock const &blk);
	NetworkConfig getConfig();
	// no need to set full node in network testing
	void setFullNode(std::shared_ptr<FullNode> full_node);
	void saveNetwork(std::string fileName);
	int getPeerCount() { return host_->peerCount();}

	// for debugging
	void setVerbose(bool verbose);
	void setDebug(bool debug);
	void print(std::string const &str);
	dev::p2p::NodeID getNodeId() { return host_->id(); };
	int getReceivedBlocksCount() {
	return taraxa_capability_->getBlocks().size();
	}

private:
	std::shared_ptr<dev::p2p::Host> host_;
	std::shared_ptr<TaraxaCapability> taraxa_capability_;

	NetworkConfig conf_;
	bool on_ = true;

	std::shared_ptr<FullNode> full_node_;

	// for debugging
	bool verbose_ = false;
	bool debug_ = false;
	std::mutex debug_mutex_;
	std::mutex verbose_mutex_;
};

} // namespace taraxa
#endif
