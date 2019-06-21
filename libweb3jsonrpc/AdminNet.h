#pragma once
#include "AdminNetFace.h"
#include "../full_node.hpp"

namespace dev
{

class NetworkFace;

namespace rpc
{

class SessionManager;

class AdminNet: public dev::rpc::AdminNetFace
{
public:
	AdminNet(std::shared_ptr<taraxa::FullNode>& _full_node);
	virtual RPCModules implementedModules() const override
	{
		return RPCModules{RPCModule{"admin", "1.0"}};
	}
	virtual bool admin_net_connect(std::string const& _node, std::string const& _session) override;
	virtual Json::Value admin_net_peers(std::string const& _session) override;
	virtual Json::Value admin_net_nodeInfo(std::string const& _session) override;
	virtual Json::Value admin_nodeInfo() override;
	virtual Json::Value admin_peers() override;
	virtual bool admin_addPeer(std::string const& _node) override;

private:
	std::weak_ptr<taraxa::FullNode> full_node_;
};
	
}
}
