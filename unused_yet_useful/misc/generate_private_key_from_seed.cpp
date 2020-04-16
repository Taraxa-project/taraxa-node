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
#include <vector>

namespace taraxa {

std::vector<std::string> generate_private_key_from_seed(
	const std::string & seed_hex, 
	size_t print_first, 
	size_t print_last
){

	constexpr auto seed_len_hex = 2 * (CryptoPP::AES::MAX_KEYLENGTH + CryptoPP::AES::BLOCKSIZE);
	std::vector<std::string> keys;
	if (seed_hex.size() != seed_len_hex) {
		std::cerr << "Seed must be " << seed_len_hex << " characters"
			<< " but " << seed_hex.size() << " given" << std::endl;
		return keys;
	}

	if (print_first > print_last) {
		std::cerr << "first (" << print_first << ") is larger than last ("
			<< print_last << ")" << std::endl;
		return keys;
	}
	CryptoPP::AES::Encryption encryption;
	const std::string key_bin = taraxa::hex2bin(seed_hex.substr(0, 2 * CryptoPP::AES::MAX_KEYLENGTH));
	encryption.SetKey(reinterpret_cast<const unsigned char*>(key_bin.data()), key_bin.size());
	std::string work_area = taraxa::hex2bin( seed_hex.substr(seed_hex.size() - 2 * CryptoPP::AES::BLOCKSIZE));
	
	for (size_t i = 0; i <= print_last; i++) {
		std::string private_key;
		encryption.ProcessBlock(reinterpret_cast<unsigned char*>(work_area.data()));
		private_key += work_area;
		encryption.ProcessBlock(reinterpret_cast<unsigned char*>(work_area.data()));
		private_key += work_area;
		if (i >= print_first) {
			try {
				const auto key = taraxa::get_public_key_hex(taraxa::bin2hex(private_key));
				keys.emplace_back(key[0]);
			} catch (const std::exception& e) {
				std::cerr << "Couldn't derive public key from private key: " << e.what() << std::endl;
				break;
			}
		}
	}
	return keys;
}
}
