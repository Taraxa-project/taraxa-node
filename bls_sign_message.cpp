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

	std::string exp_hex;

	boost::program_options::options_description options(
		"Signs hex encoded message given on standard input\n"
		"Prints hex encoded signature to standard output.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private key to sign with (hex)");

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

	std::array<uint64_t, bls::local::keySize> exp_temp{};
	std::memcpy(exp_temp.data(), exp_bin.data(), exp_bin.size());

	bls::SecretKey secret_key;
	secret_key.set(exp_temp.data());

	std::string message_hex;
	std::cin >> message_hex;

	bls::Signature signature;
	const std::string message_bin = taraxa::hex2bin(message_hex);
	secret_key.sign(signature, message_bin.data(), message_bin.size());

	std::string signature_bin;
	signature.getStr(signature_bin, bls::IoFixedByteSeq);
	std::cout << taraxa::bin2hex(signature_bin) << std::endl;

	return EXIT_SUCCESS;
}
