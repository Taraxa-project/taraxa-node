/*
Copyright 2018 Ilja Honkonen
*/
#include <cstdlib>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include "create_receive.hpp"

int main(int argc, char* argv[]) {

	/*
	Parse command line options and validate input
	*/

	std::string exp_hex, send_hex, previous_hex;

	boost::program_options::options_description options(
		"Creates a receive transaction corresponding to a send, ready to be "
		"sent over the network.\nAll hex encoded strings must be given without "
		"the leading 0x.\nUsage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("previous", boost::program_options::value<std::string>(&previous_hex),
			"Hash of previous transaction from the accepting account (hex)")
		("send", boost::program_options::value<std::string>(&send_hex),
			"Hash of send transaction that is accepted by the receive (hex)")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private key used to sign the receive (hex)");

	boost::program_options::variables_map option_variables;
	boost::program_options::store(
		boost::program_options::parse_command_line(argc, argv, options),
		option_variables
	);
	boost::program_options::notify(option_variables);

	if (option_variables.count("help") > 0) {
		std::cout << options << std::endl;
		return EXIT_SUCCESS;
	}

	std::string blk = taraxa::create_receive(exp_hex, previous_hex, send_hex);
	//std::string blk;
	if (blk.empty()) {
		std::cout<<"Create receive block fail ..."<<std::endl;
	return EXIT_FAILURE;
	}
	else{
		std::cout<<blk<<std::endl;
		return EXIT_SUCCESS;
	}
}
