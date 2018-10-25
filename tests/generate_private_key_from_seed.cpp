/*
Copyright 2018 Ilja Honkonen
*/


#include "generate_private_key_from_seed.hpp"
#include <boost/program_options.hpp>
#include <cryptopp/aes.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>


int main(int argc, char* argv[]) {
	constexpr auto seed_len_hex = 2 * (CryptoPP::AES::MAX_KEYLENGTH + CryptoPP::AES::BLOCKSIZE);

	std::string seed_hex;
	size_t print_first = 0, print_last = 0;

	boost::program_options::options_description options(
		"Prints deterministic private keys to standard output,\n"
		"based on given seed, which can be given e.g.\n"
		"to print_key_info.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("seed",
			boost::program_options::value<std::string>(&seed_hex),
			("Seed for private key generation (hex, "
			+ std::to_string(seed_len_hex) + " characters)").c_str())
		("first",
			boost::program_options::value<size_t>(&print_first),
			"First private key to print, starting from 0")
		("last",
		 	boost::program_options::value<size_t>(&print_last),
			"Last private key to print, starting from 0");

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

	std::vector<std::string> keys = taraxa::generate_private_key_from_seed(seed_hex, print_first, print_last);
	if (keys.empty()){
		std::cout<<"Cannot generate private keys from seed ..."<<std::endl;
		return EXIT_FAILURE;
	}
	else {
		for (auto k:keys) std::cout<<k<<std::endl;
	}
	return EXIT_SUCCESS;
}
