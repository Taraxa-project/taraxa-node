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

	std::string exp_hex;

	boost::program_options::options_description options(
		"Prints information about given private key.\n"
		"Input and output is hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private key to print (hex)");

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

	if (exp_bin.size() != bls::local::keySize * sizeof(uint64_t)) {
		std::cerr << "Private key must be " << bls::local::keySize * sizeof(uint64_t)
			<< " bytes but is " << exp_bin.size()
			<< " after decoding hex string" << std::endl;
		return EXIT_FAILURE;
	}

	bls::SecretKey secret_key;
	secret_key.setStr(exp_bin, bls::IoFixedByteSeq);

	bls::PublicKey public_key;
	secret_key.getPublicKey(public_key);

	std::string public_key_bin;
	public_key.getStr(public_key_bin, bls::IoFixedByteSeq);
	// TODO? std::reverse(public_key_bin.begin(), public_key_bin.end());

	std::cout << "Private key: " << taraxa::bin2hex(taraxa::hex2bin(exp_hex)) << std::endl;
	std::cout << "Public key:  " << taraxa::bin2hex(public_key_bin) << std::endl;

	return EXIT_SUCCESS;
}
