/*
Copyright 2018 Ilja Honkonen
*/


#include "signatures.hpp"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	std::string input_bin;

	boost::program_options::options_description options(
		"Encodes give binary string as hex (without leading 0x).\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("input",
		 	boost::program_options::value<std::string>(&input_bin),
			"Binary string to encode");

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
		const auto input_hex = taraxa::bin2hex(input_bin);
		std::cout << input_hex << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Couldn't encode input: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
