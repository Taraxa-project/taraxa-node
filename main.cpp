/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:03:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-04 13:02:10
 */
 
#include <iostream> 
#include <boost/asio.hpp>
#include "full_node.hpp"
#include "rpc.hpp"
#include "wallet.hpp"
//#include "util.hpp"

int main(int argc, char *argv[]){
	
	if (argc!=4){
		std::cout<<"Please config file [conf_full_node] [conf_wallet] [conf_rpc]..."<<std::endl;
		return 0;
	}
	std::string conf_full_node = argv[1];
	std::string conf_wallet = argv[2];
	std::string conf_rpc = argv[3];
	try{
		std::shared_ptr<taraxa::FullNode> node (new taraxa::FullNode(conf_full_node));
		node->setVerbose(true);
		std::cout<<"Full node is set"<<std::endl;
		std::string action;
		std::shared_ptr<taraxa::Wallet> wallet (new taraxa::Wallet(conf_wallet));
		boost::asio::io_context context;
		std::shared_ptr<taraxa::Rpc> rpc (new taraxa::Rpc(conf_rpc, context, *node, *wallet));
		rpc->start();
		context.run();
		return 1;
	}
	catch(std::exception &e){
		std::cerr<<e.what();
	}
}