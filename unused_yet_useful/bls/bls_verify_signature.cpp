/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"

#include <bls/bls.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	bls::init();

	std::string pubkey_hex, signature_hex, message_hex;

	boost::program_options::options_description options(
		"Verify a signature agains public key.\n"
		"Input and output is hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("pubkey", boost::program_options::value<std::string>(&pubkey_hex),
			"Public key to use for verification")
		("signature", boost::program_options::value<std::string>(&signature_hex),
			"Signature to verify")
		("message", boost::program_options::value<std::string>(&message_hex),
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

	if (pubkey_hex.size() != 4 * bls::local::keySize * sizeof(uint64_t)) {
		std::cerr << "Public key must be " << 4 * bls::local::keySize * sizeof(uint64_t) << " characters." << std::endl;
		return EXIT_FAILURE;
	}

	const std::string pubkey_bin = taraxa::hex2bin(pubkey_hex);
	if (pubkey_bin.size() != 2 * bls::local::keySize * sizeof(uint64_t)) {
		std::cerr << "Decoded public key must be " << 2 * bls::local::keySize * sizeof(uint64_t) << " bytes." << std::endl;
		return EXIT_FAILURE;
	}

	bls::PublicKey public_key;
	try {
		public_key.setStr(pubkey_bin, bls::IoFixedByteSeq);
	} catch (...) {
		std::cerr << "Couldn't set public key." << std::endl;
		return EXIT_FAILURE;
	}

	if (signature_hex.size() == 0) {
		std::cerr << "Signature cannot be empty." << std::endl;
		return EXIT_FAILURE;
	}

	bls::Signature signature;
	try {
		signature.setStr(taraxa::hex2bin(signature_hex), bls::IoFixedByteSeq);
	} catch (...) {
		std::cerr << "Couldn't set signature." << std::endl;
		return EXIT_FAILURE;
	}

	const std::string message_bin = taraxa::hex2bin(message_hex);
	
	if (signature.verify(public_key, message_bin)) {
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
