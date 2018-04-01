/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"
#include "signatures.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	std::string exp_hex, send_hex, previous_hex;

	boost::program_options::options_description options(
		"Creates a receive transaction corresponding to a send, ready to be "
		"sent over the network.\nAll hex encoded strings must be given without "
		"the leading 0x.\nUsage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("previous", boost::program_options::value<std::string>(&previous_hex),
			"Hash of previous transaction from the accepting account (hex)")
		("send", boost::program_options::value<std::string>(&send_hex),
			"Hash of send transaction that is accepted by the receive (hex)")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private exponent of the key used to sign the receive (hex)");

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

	if (exp_hex.size() != 64) {
		std::cerr << "Key must be 64 characters but is " << exp_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		std::cerr << "Hash of previous transaction must be " << nr_hash_chars
			<< " characters but is " << previous_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	if (send_hex.size() != nr_hash_chars) {
		std::cerr << "Hash of send must be " << nr_hash_chars
			<< " characters but is " << send_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	const std::string
		exp_bin = taraxa::hex2bin(exp_hex),
		send_bin = taraxa::hex2bin(send_hex),
		previous_bin = taraxa::hex2bin(previous_hex);

	const auto signature = taraxa::sign_message(previous_bin + send_bin, exp_bin);

	std::cout << taraxa::bin2hex(signature) << " "
		<< taraxa::bin2hex(previous_bin) << " "
		<< taraxa::bin2hex(send_bin) << std::endl;

	return EXIT_SUCCESS;
}
