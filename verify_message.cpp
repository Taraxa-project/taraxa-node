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

	std::string x_hex, y_hex, message_hex;

	boost::program_options::options_description options(
		"Verify a signature given on standard input.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("x", boost::program_options::value<std::string>(&x_hex),
			"Verify using arg as the x coordinate of public point "
			"given as hex encoded string without the leading 0x")
		("y", boost::program_options::value<std::string>(&y_hex),
			"Verify using arg as the y coordinate of public point "
			"given as hex encoded string without the leading 0x")
		("message", boost::program_options::value<std::string>(&message_hex),
			"Verify arg given as hex encoded string without the leading 0x");

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

	std::string signature_hex;
	std::cin >> signature_hex;
	const bool verified = taraxa::verify_signature_hex(signature_hex, message_hex, x_hex, y_hex);

	if (verified) {
		if (option_variables.count("verbose") > 0) {
			std::cout << "PASS" << std::endl;
		}
		return EXIT_SUCCESS;
	} else {
		if (option_variables.count("verbose") > 0) {
			std::cout << "FAIL" << std::endl;
		}
		return EXIT_FAILURE;
	}
}
