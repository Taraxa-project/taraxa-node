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

	size_t A = 2, B = 2;
	boost::program_options::options_description options(
		"Prints on each line of standard output one id, "
		"private and public key for A out of B threshold "
		"of private key read from standard input.\n"
		"Input and output is hex encoded without leading 0x\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("A", boost::program_options::value<size_t>(&A),
			"Minimum number of shares required for valid signature (> 1)")
		("B", boost::program_options::value<size_t>(&B),
			"Total number of shares to create (> 1)");

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

	if (A < 2) {
		std::cerr << "Minimum number of shares must be at least 2." << std::endl;
		return EXIT_FAILURE;
	}

	if (B < 2) {
		std::cerr << "Total number of shares must be at least 2." << std::endl;
		return EXIT_FAILURE;
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

	bls::SecretKeyVec master_secret_key;
	secret_key.getMasterSecretKey(master_secret_key, A);

	bls::SecretKeyVec secret_keys(B);
	bls::IdVec ids(B);
	for (size_t i = 0; i < B; i++) {
		int id = i + 1;
		ids[i] = id;
		secret_keys[i].set(master_secret_key, id);
	}
	for (size_t i = 0; i < B; i++) {
		std::string secret_key_bin;
		secret_keys[i].getStr(secret_key_bin, bls::IoFixedByteSeq);
		std::reverse(secret_key_bin.begin(), secret_key_bin.end());

		bls::PublicKey public_key;
		secret_keys[i].getPublicKey(public_key);
		std::string public_key_bin;
		public_key.getStr(public_key_bin, bls::IoFixedByteSeq);

		std::string id_bin;
		ids[i].getStr(id_bin, bls::IoFixedByteSeq);
		std::reverse(id_bin.begin(), id_bin.end());

		std::cout << taraxa::bin2hex(id_bin) << " "
			<< taraxa::bin2hex(secret_key_bin) << " "
			<< taraxa::bin2hex(public_key_bin) << std::endl;
	}

	return EXIT_SUCCESS;
}
