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
	std::string key_hex, proof_hex, message_hex;

	boost::program_options::options_description options(
		"Verifies VRF proof of message using public key.\n"
		"Input and output must be hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("pubkey",
		 	boost::program_options::value<std::string>(&key_hex),
			"Public key for verifying proof")
		("proof",
		 	boost::program_options::value<std::string>(&proof_hex),
			"VRF proof to verify")
		("message",
		 	boost::program_options::value<std::string>(&message_hex),
			"Message to verify");

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

	if (key_hex.size() != 2 * crypto_vrf_PUBLICKEYBYTES) {
		std::cerr << "Public key must be " << 2 * crypto_vrf_PUBLICKEYBYTES << " characters"
			<< " but " << key_hex.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string key_bin = taraxa::hex2bin(key_hex);
	if (key_bin.size() != crypto_vrf_PUBLICKEYBYTES) {
		std::cerr << "Public key must be " << crypto_vrf_PUBLICKEYBYTES << " bytes"
			<< " but " << key_bin.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	if (proof_hex.size() != 2 * crypto_vrf_PROOFBYTES) {
		std::cerr << "Proof must be " << 2 * crypto_vrf_PROOFBYTES << " characters"
			<< " but " << proof_hex.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string proof_bin = taraxa::hex2bin(proof_hex);
	if (proof_bin.size() != crypto_vrf_PROOFBYTES) {
		std::cerr << "Proof must be " << crypto_vrf_PROOFBYTES << " bytes"
			<< " but " << proof_bin.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string message_bin = taraxa::hex2bin(message_hex);

	std::string output_bin(crypto_vrf_OUTPUTBYTES, ' ');
	if (
		crypto_vrf_verify(
			(unsigned char*) output_bin.data(),
			(unsigned char*) key_bin.data(),
			(unsigned char*) proof_bin.data(),
			(unsigned char*) message_bin.data(),
			message_bin.size()
		) != 0
	) {
		if (option_variables.count("verbose") > 0) {
			std::cout << "FAIL" << std::endl;
		}
		return EXIT_FAILURE;
	}

	if (option_variables.count("verbose") > 0) {
		std::cout << "PASS" << std::endl;
	}
	return EXIT_SUCCESS;
}
