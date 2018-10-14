/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"
#include "signatures.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/aes.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {
	constexpr auto seed_len_hex = 2 * (CryptoPP::AES::MAX_KEYLENGTH + CryptoPP::AES::BLOCKSIZE);

	std::string seed_hex;
	size_t print_first = 0, print_last = 0;

	boost::program_options::options_description options(
		"Prints deterministic private keys to standard output\n"
		"based on given seed, which can be given e.g.\n"
		"to print_key_info.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("seed",
		 	boost::program_options::value<std::string>(&seed_hex),
			("Seed for private key generation (hex, "
			+ std::to_string(seed_len_hex) + " characters)").c_str())
		("first",
		 	boost::program_options::value<size_t>(&print_first),
			"First private key to print, starting from 0")
		("last",
		 	boost::program_options::value<size_t>(&print_last),
			"Last private key to print, starting from 0");

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

	if (seed_hex.size() != seed_len_hex) {
		std::cerr << "Seed must be " << seed_len_hex << " characters"
			<< " but " << seed_hex.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	if (print_first > print_last) {
		std::cerr << "first (" << print_first << ") is larger than last ("
			<< print_last << ")" << std::endl;
		return EXIT_FAILURE;
	}

	CryptoPP::AES::Encryption encryption;
	const std::string key_bin = taraxa::hex2bin(seed_hex.substr(0, 2 * CryptoPP::AES::MAX_KEYLENGTH));
	encryption.SetKey(reinterpret_cast<const unsigned char*>(key_bin.data()), key_bin.size());

	std::string work_area = taraxa::hex2bin(
		seed_hex.substr(seed_hex.size() - 2 * CryptoPP::AES::BLOCKSIZE)
	);
	for (size_t i = 0; i <= print_last; i++) {
		std::string private_key;
		encryption.ProcessBlock(reinterpret_cast<unsigned char*>(work_area.data()));
		private_key += work_area;
		encryption.ProcessBlock(reinterpret_cast<unsigned char*>(work_area.data()));
		private_key += work_area;
		if (i >= print_first) {
			try {
				const auto keys = taraxa::get_public_key_hex(taraxa::bin2hex(private_key));
				std::cout << keys[0] << std::endl;
			} catch (const std::exception& e) {
				std::cerr << "Couldn't derive public key from private key: " << e.what() << std::endl;
				return EXIT_FAILURE;
			}
		}
	}

	return EXIT_SUCCESS;
}
