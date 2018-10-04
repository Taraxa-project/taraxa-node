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

	boost::program_options::options_description options(
		"Prints information about hex encoded (without leading 0x) "
		"private key read from standard input.\nUsage: program_name "
		"[options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit");

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

	std::string exp_hex;
	std::cin >> exp_hex;

	if (exp_hex.size() != 2 * bls::local::keySize * sizeof(uint64_t)) {
		std::cerr << "Private key must be " << 2 * bls::local::keySize * sizeof(uint64_t) << " characters" << std::endl;
		return EXIT_FAILURE;
	}

	std::string exp_bin = taraxa::hex2bin(exp_hex);
	std::reverse(exp_bin.begin(), exp_bin.end());

	if (exp_bin.size() != bls::local::keySize * sizeof(uint64_t)) {
		std::cerr << "Private key must be " << bls::local::keySize * sizeof(uint64_t)
			<< " bytes but is " << exp_bin.size()
			<< " after decoding hex string" << std::endl;
		return EXIT_FAILURE;
	}

	std::array<uint64_t, bls::local::keySize> exp_temp;
	std::memcpy(exp_temp.data(), exp_bin.data(), exp_bin.size());

	bls::SecretKey secret_key;
	secret_key.set(exp_temp.data());

	bls::PublicKey public_key;
	secret_key.getPublicKey(public_key);

	std::cout << "Private key: " << secret_key << std::endl;
	std::cout << "Public key:  " << public_key << std::endl;

	return EXIT_SUCCESS;
}
