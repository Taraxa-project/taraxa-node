#include "network.hpp"
#include "full_node.hpp"
#include "libp2p/Host.h"
#include "taraxa_capability.h"
#include "visitor.hpp"
#include <libdevcore/Log.h>
#include <libdevcrypto/Common.h>
#include <libp2p/Network.h>

namespace taraxa {

// NetworkCongif ------------------------------

NetworkConfig::NetworkConfig(std::string const &json_file)
	: json_file_name(json_file) {
	boost::property_tree::ptree doc = loadJsonFile(json_file);
	try {
	network_listen_port = doc.get<uint16_t>("network_listen_port");
	network_node_id = doc.get<std::string>("network_node_id");
	for (auto &item : doc.get_child("network_boot_nodes")) {
		NodeConfig node;
		node.id = item.second.get<std::string>("id");
		node.ip = item.second.get<std::string>("ip");
		node.port = item.second.get<uint16_t>("port");
		network_boot_nodes.push_back(node);
	}
	} catch (std::exception &e) {
	std::cerr << e.what() << std::endl;
	}
}

// Network ----------------------------------------

Network::Network(std::string const &conf_file_name) : Network(conf_file_name, ""){
}

Network::Network(std::string const &conf_file_name, std::string networkFile) try : conf_(conf_file_name),
	full_node_(nullptr) {
	dev::LoggingOptions logOptions;
	logOptions.verbosity = dev::VerbositySilent;
	dev::setupLogging(logOptions);
	auto key = dev::KeyPair::create();
	if (conf_.network_node_id.empty()) {
	printf("New key generated %s\n", toHex(key.secret().ref()).c_str());
	} else {
	auto secret =
		dev::Secret(conf_.network_node_id,
			dev::Secret::ConstructFromStringType::FromHex);
	key = dev::KeyPair(secret);
	}
	if(conf_file_name.empty()) {
	host_ = std::make_shared<dev::p2p::Host>(
		"TaraxaNode", key,
		dev::p2p::NetworkConfig("127.0.0.1", conf_.network_listen_port, false,
			true));
	}
	else {
		auto networkData = contents(networkFile);
		host_ = std::make_shared<dev::p2p::Host>(
			"TaraxaNode",
			dev::p2p::NetworkConfig("127.0.0.1", conf_.network_listen_port, false,
				true), dev::bytesConstRef(&networkData));
	}
	taraxa_capability_ = std::make_shared<TaraxaCapability>(*host_.get());
	host_->registerCapability(taraxa_capability_);
} catch (std::exception &e) {
	std::cerr << "Construct Network Error ... " << e.what() << "\n";
	throw e;
}

Network::~Network() {}

void Network::setFullNode(std::shared_ptr<FullNode> full_node) {
	full_node_ = full_node;
	taraxa_capability_->setFullNode(full_node);
}

NetworkConfig Network::getConfig() { return conf_; }
void Network::start() {
	if (verbose_) {
	std::cout << "Network started";
	}
	host_->start();
	printf("Started Node id: %s\n", host_->id().hex().c_str());

	for (auto &node : conf_.network_boot_nodes) {
	printf("Adding node\n");
	host_->addNode(
		dev::Public(node.id),
		dev::p2p::NodeIPEndpoint(bi::address::from_string(node.ip.c_str()),
			node.port, node.port));
	}
	on_ = true;
}

void Network::stop() {
	host_->stop();
	on_ = false;
}

void Network::setVerbose(bool verbose) { verbose_ = verbose; }
void Network::setDebug(bool debug) { debug_ = debug; }

void Network::print(std::string const &str) {
	std::unique_lock<std::mutex> lock(verbose_mutex_);
	std::cout << str;
}

void Network::sendTest(NodeID const &id) {
	taraxa_capability_->sendTestMessage(id, 1);
	if (verbose_) {
		print("Sent ===> ");
	}
}

void Network::sendBlock(NodeID const &id, StateBlock const &blk) {
	taraxa_capability_->sendBlock(id, blk);
	if (verbose_) {
		print("Sent Block");
	}
}

void Network::onNewBlock(StateBlock const &blk) {
	taraxa_capability_->onNewBlock(blk);
	if (verbose_) {
		print("On new block");
	}
}

void Network::saveNetwork(std::string fileName){
	auto netData = host_->saveNetwork();
	if (!netData.empty())
		writeFile(fileName, &netData);
}

} // namespace taraxa
