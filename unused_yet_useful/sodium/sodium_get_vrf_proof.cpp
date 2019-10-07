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
	std::string key_hex, message_hex;

	boost::program_options::options_description options(
		"Prints VRF proof of message using secret key.\n"
		"Input and output must be hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key",
		 	boost::program_options::value<std::string>(&key_hex),
			"Secret key for generating proof")
		("message",
		 	boost::program_options::value<std::string>(&message_hex),
			"Message to prove");

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

	if (key_hex.size() != 2 * crypto_vrf_SECRETKEYBYTES) {
		std::cerr << "Secret key must be " << 2 * crypto_vrf_SECRETKEYBYTES << " characters"
			<< " but " << key_hex.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string key_bin = taraxa::hex2bin(key_hex);
	if (key_bin.size() != crypto_vrf_SECRETKEYBYTES) {
		std::cerr << "Secret key must be " << crypto_vrf_SECRETKEYBYTES << " bytes"
			<< " but " << key_bin.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string message_bin = taraxa::hex2bin(message_hex);

	std::string proof_bin(crypto_vrf_PROOFBYTES, ' ');
	if (
		crypto_vrf_prove(
			(unsigned char*) proof_bin.data(),
			(unsigned char*) key_bin.data(),
			(unsigned char*) message_bin.data(),
			message_bin.size()
		) != 0
	) {
		std::cerr << "Couldn't get proof." << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << taraxa::bin2hex(proof_bin) << std::endl;

	return EXIT_SUCCESS;
}
