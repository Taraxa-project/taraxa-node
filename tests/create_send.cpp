/*
Copyright 2018 Ilja Honkonen
*/
#include <cstdlib>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include "create_send.hpp"

int main(int argc, char* argv[]) {

	/*
	Parse command line options and validate input
	*/

	std::string exp_hex, receiver_hex, new_balance_str, payload_hex, previous_hex;

	boost::program_options::options_description options(
		"Creates an outgoing transaction ready to be sent over the network.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("previous", boost::program_options::value<std::string>(&previous_hex),
			"Hash of previous transaction from the account (hex)")
		("receiver", boost::program_options::value<std::string>(&receiver_hex),
			"Receiver of the transaction that must create to corresponding receive (hex)")
		("payload", boost::program_options::value<std::string>(&payload_hex),
			"Payload of the transaction (hex)")
		("new-balance", boost::program_options::value<std::string>(&new_balance_str),
			"Balance of sending account after the transaction, given as (decimal) number.")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private exponent of the key used to sign the transaction (hex)");

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

	
	std::string blk = taraxa::create_send(exp_hex, receiver_hex, new_balance_str, payload_hex, previous_hex);
	
	if (blk.empty()){
		std::cout<<"Create send block fail ..."<<std::endl;
		return EXIT_FAILURE;
	}
	else {
		std::cout << blk << std::endl;
	}
	return EXIT_SUCCESS;
}
