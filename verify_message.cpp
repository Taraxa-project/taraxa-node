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

	std::string pubkey_hex, message_hex;

	boost::program_options::options_description options(
		"Verify a signature given on standard input.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("pubkey", boost::program_options::value<std::string>(&pubkey_hex),
			"Public key against which to verify a message (hex)")
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

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey zero_key;
	zero_key.Initialize(CryptoPP::ASN1::secp256r1(), CryptoPP::ECP::Point());
	const auto point_size = 2 * zero_key.GetGroupParameters().GetCurve().EncodedPointSize(true);

	if (pubkey_hex.size() != point_size) {
		std::cerr << "Hex encoded public key key must be " << point_size
			<< " characters but is " << pubkey_hex.size() << std::endl;
		return EXIT_FAILURE;
	}


	std::string signature_hex;
	std::cin >> signature_hex;
	const bool verified = taraxa::verify_signature_hex(signature_hex, message_hex, pubkey_hex);

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
