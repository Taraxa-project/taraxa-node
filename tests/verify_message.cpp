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

	std::string pubkey_hex, message_hex, signature_hex;

	boost::program_options::options_description options(
		"Verify signature of message using public key.\n"
		"Input and output is hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("pubkey", boost::program_options::value<std::string>(&pubkey_hex),
			"Public key against which to verify message")
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

	CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey zero_key;
	zero_key.Initialize(CryptoPP::ASN1::secp256r1(), CryptoPP::ECP::Point());
	const auto point_size = 2 * zero_key.GetGroupParameters().GetCurve().EncodedPointSize(true);

	if (pubkey_hex.size() != point_size) {
		std::cerr << "Hex encoded public key key must be " << point_size
			<< " characters but is " << pubkey_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

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
