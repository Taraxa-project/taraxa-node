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

	std::string pubkey_hex, message_hex;

	boost::program_options::options_description options(
		"Verify a signature given on standard input (as printed by bls_sign_message).\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("pubkey", boost::program_options::value<std::string>(&pubkey_hex),
			"Public key against which to verify a message (as printed by bls_print_key_info)")
		("message", boost::program_options::value<std::string>(&message_hex),
			"Message to verify (hex)");

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

	bls::PublicKey public_key;
	try {
		public_key.setStr(taraxa::hex2bin(pubkey_hex), bls::IoFixedByteSeq);
	} catch (...) {
		std::cerr << "Couldn't set public key." << std::endl;
		return EXIT_FAILURE;
	}

	std::string signature_hex;
	std::cin >> signature_hex;

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
