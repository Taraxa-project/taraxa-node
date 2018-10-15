/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/aes.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <sodium.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {
	std::string seed_hex;

	boost::program_options::options_description options(
		"Prints deterministic private key to standard output,\n"
		"based on given seed, which can be given e.g.\n"
		"to print_key_info.\n"
		"Input and output must be hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("seed",
		 	boost::program_options::value<std::string>(&seed_hex),
			"Seed for private key generation");

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

	if (seed_hex.size() != 2 * crypto_vrf_SEEDBYTES) {
		std::cerr << "Seed must be " << 2 * crypto_vrf_SEEDBYTES << " characters"
			<< " but " << seed_hex.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string seed_bin = taraxa::hex2bin(seed_hex);
	if (seed_bin.size() != crypto_vrf_SEEDBYTES) {
		std::cerr << "Seed must be " << crypto_vrf_SEEDBYTES << " bytes"
			<< " but " << seed_bin.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	std::string
		secret_key_bin(crypto_vrf_SECRETKEYBYTES, ' '),
		public_key_bin(crypto_vrf_PUBLICKEYBYTES, ' ');

	crypto_vrf_keypair_from_seed(
		(unsigned char*) public_key_bin.data(),
		(unsigned char*) secret_key_bin.data(),
		(unsigned char*) seed_bin.data()
	);
	secret_key_bin = secret_key_bin.substr(secret_key_bin.size() / 2);

	std::cout << taraxa::bin2hex(secret_key_bin) << std::endl;

	return EXIT_SUCCESS;
}
