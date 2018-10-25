/*
Copyright 2018 Ilja Honkonen
*/
#include "bin2hex2bin.hpp"
#include "signatures.hpp"
#include <boost/program_options.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {

	std::string exp_hex, message_hex;

	boost::program_options::options_description options(
		"Prints signature for message created with key.\n"
		"Input and output is hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private key to sign with")
		("message", boost::program_options::value<std::string>(&message_hex),
			"Message to sign");

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
		std::cout << taraxa::sign_message_hex(message_hex, exp_hex) << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Couldn't sign message: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
