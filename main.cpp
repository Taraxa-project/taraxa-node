/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:03:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-30 11:20:41
 */
 
#include <iostream> 
#include <boost/asio.hpp>
#include "full_node.hpp"
#include "rpc.hpp"
//#include "util.hpp"

int main(int argc, char *argv[]){
	
	if (argc!=6){
		std::cout<<"Please enter host ip, port, path of account db, path of block db, other port"<<std::endl;
		return 0;
	}
	string ip=argv[1];
	unsigned port = std::stoi(string(argv[2]));
	string db_accounts_path = argv[3];
	string db_blocks_path = argv[4];
	unsigned other_port = std::stoi(string(argv[5]));
	std::shared_ptr<taraxa::FullNode> node (new taraxa::FullNode(ip, port, db_accounts_path, db_blocks_path));
	node->setVerbose(true);
	node->addRemotes(ip, other_port);
	std::cout<<"Full node is set"<<std::endl;
	string action;

	boost::asio::io_context context;
	taraxa::RpcConfig rpc_conf("conf_rpc.json");
	std::shared_ptr<taraxa::Rpc> rpc (new taraxa::Rpc(context, *node, rpc_conf));
	rpc->start();


	// unsigned from, to;
	// while (true){
	// 	cout<<"Action [c=create account, s=send, e=exit]"<<endl;
	// 	cin>>action>>from>>to;
	// 	if (action == "c"){
	// 		// from: idx, to: init balance
	// 		fnode.createAccount(from, to);
	// 	} else if (action == "s"){
	// 		fnode.sendBlock(from, to, 1);
	// 	} 
	// 	else if (action == "e"){
	// 		break;
	// 	}	
	// }
	context.run();
	return 1;
}