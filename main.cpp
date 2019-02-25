/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-29 15:03:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-25 12:03:37
 */
 
#include <iostream> 
#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "full_node.hpp"
#include "rpc.hpp"
#include "wallet.hpp"

int main(int argc, char *argv[]){
	
	bool verbose = false;
	std::string conf_full_node, conf_network, conf_rpc;

	boost::program_options::options_description options(
		"Usage: main [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print more info")
		("conf_full_node",
			boost::program_options::value<std::string>(&conf_full_node),
			"Configure file for full node [required]")
		("conf_network",
			boost::program_options::value<std::string>(&conf_network),
			"Configure file for network [required]")
		("conf_rpc",
			boost::program_options::value<std::string>(&conf_rpc),
			"Configure file for rpc [required]");

	boost::program_options::variables_map option_vars;
	boost::program_options::store(
		boost::program_options::parse_command_line(argc, argv, options), 
		option_vars
	);
	boost::program_options::notify(option_vars);
	
	if (option_vars.count("help")){
		std::cout << options <<std::endl;
		return 1;
	}
	if (option_vars.count("verbose") > 0) {
		verbose = true;
	}
	if (!option_vars.count("conf_network")) {
		std::cout<<"Please specify full node configuration file [--conf_full_node]..."<<std::endl;
		return 0;
	}
	if (!option_vars.count("conf_network")) {
		std::cout<<"Please specify network configuration file ... [--conf_network]"<<std::endl;
		return 0;
	}
	if (!option_vars.count("conf_rpc")) {
		std::cout<<"Please specify rpc configuration file ... [--conf_rpc]"<<std::endl;
		return 0;
	}
	
	boost::asio::io_context context;

	try{
		auto node (std::make_shared<taraxa::FullNode>(context, conf_full_node, conf_network));
		
		node->setVerbose(verbose);
		node->start();
		std::string action;		
		auto rpc (std::make_shared<taraxa::Rpc>(context, conf_rpc, node->getShared()));
		rpc->start();
		context.run();
		
		return 1;
	}
	catch(std::exception &e){
		std::cerr<<e.what();
	}
}