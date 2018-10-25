/*
Copyright 2018 Ilja Honkonen
*/

#include "generate_private_key.hpp"
#include <iostream>
#include <string>
#include <boost/program_options.hpp>

int main(int argc, char* argv[]) {

	boost::program_options::options_description options(
		"Prints a random private key to standard output, "
		"which can be given e.g. to print_key_info.\n"
		"Usage: program_name [options], where options are:"
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
	std::string key = taraxa::generate_private_key();
	if (key.empty()){
		std::cout<<"Genereate private key fail ..."<<std::endl;
	return EXIT_FAILURE;
	}
	else{
		std::cout<<key<<std::endl;
	}
	return EXIT_SUCCESS;
}
