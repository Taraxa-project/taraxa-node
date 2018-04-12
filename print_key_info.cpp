/*
Copyright 2018 Ilja Honkonen
*/


#include "signatures.hpp"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	boost::program_options::options_description options(
		"Prints information about hex encoded (without leading 0x) "
		"private key read from standard input.\nUsage: program_name "
		"[options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit");

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

	std::string exp_hex;
	std::cin >> exp_hex;

	try {
		const auto keys = taraxa::get_public_key_hex(exp_hex);
		std::cout << "Private key:                " << keys[0] << std::endl;
		std::cout << "Public key, first element:  " << keys[1] << std::endl;
		std::cout << "Public key, second element: " << keys[2] << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Couldn't derive public key from private key: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
