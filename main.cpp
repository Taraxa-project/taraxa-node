/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:03:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-29 13:49:03
 */
 
#include <iostream> 
#include <boost/asio.hpp>
#include "full_node.hpp"
#include "rpc.hpp"
#include "wallet.hpp"

int main(int argc, char *argv[]){
	
	if (argc!=4){
		std::cout<<"Please config file [conf_full_node] [conf_network] [conf_rpc] ..."<<std::endl;
		return 0;
	}
	std::string conf_full_node = argv[1];
	std::string conf_network = argv[2];
	std::string conf_rpc = argv[3];
	boost::asio::io_context context;

	try{
		auto node (std::make_shared<taraxa::FullNode>(context, conf_full_node, conf_network));
		node->setVerbose(true);
		node->start();
		std::cout<<"Full node is set"<<std::endl;
		std::string action;		
		auto rpc (std::make_shared<taraxa::Rpc>(context, conf_rpc, node->getShared()));
		rpc->start();
		std::cout<<"Rpc is set"<<std::endl;
		context.run();
		
		return 1;
	}
	catch(std::exception &e){
		std::cerr<<e.what();
	}
}