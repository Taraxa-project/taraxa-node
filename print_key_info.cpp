/*
Copyright 2018 Ilja Honkonen
*/


#include "signatures.hpp"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	std::string key_hex;

	boost::program_options::options_description options(
		"Prints information about private key.\n"
		"Input and output is hex encoded without leading 0x\n."
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key", boost::program_options::value<std::string>(&key_hex),
			"Public key against which to verify message");

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

	try {
		const auto keys = taraxa::get_public_key_hex(key_hex);
		std::cout << "Private key: " << keys[0] << std::endl;
		std::cout << "Public key:  " << keys[1] << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Couldn't derive public key from private key: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
