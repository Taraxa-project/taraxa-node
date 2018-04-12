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

	std::string exp_hex;

	boost::program_options::options_description options(
		"Sign a hex encoded message without the leading 0x given on "
		"standard input using a key produced with generate_private_key, "
		"for example. Prints the hex encoded result to standard output.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Sign using arg as the private exponent of the key "
			"given as hex encoded string without the leading 0x");

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

	if (exp_hex.size() != 64) {
		std::cerr << "Key must be 64 characters but is " << exp_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	std::string message_hex;
	std::cin >> message_hex;
	const auto signature = taraxa::sign_message_hex(message_hex, exp_hex);

	std::cout << taraxa::bin2hex(signature);

	return EXIT_SUCCESS;
}
