/*
Copyright 2018 Ilja Honkonen
*/

#include "bin2hex2bin.hpp"

#include <bls/bls.hpp>
#include <boost/program_options.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	bls::init();

	std::string exp_hex, message_hex;

	boost::program_options::options_description options(
		"Signs a message with private key.\n"
		"Input and output is hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private key to sign with")
		("message", boost::program_options::value<std::string>(&message_hex),
			"Message to sign");

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

	if (exp_hex.size() != 2 * bls::local::keySize * sizeof(uint64_t)) {
		std::cerr << "Private key must be " << 2 * bls::local::keySize * sizeof(uint64_t) << " characters" << std::endl;
		return EXIT_FAILURE;
	}

	std::string exp_bin = taraxa::hex2bin(exp_hex);
	std::reverse(exp_bin.begin(), exp_bin.end());

	bls::SecretKey secret_key;
	secret_key.setStr(exp_bin, bls::IoFixedByteSeq);

	bls::Signature signature;
	const std::string message_bin = taraxa::hex2bin(message_hex);
	secret_key.sign(signature, message_bin.data(), message_bin.size());

	std::string signature_bin;
	signature.getStr(signature_bin, bls::IoFixedByteSeq);
	std::cout << taraxa::bin2hex(signature_bin) << std::endl;

	return EXIT_SUCCESS;
}
