/*
Copyright 2018 Ilja Honkonen
*/

#ifndef LEDGER_STORAGE_HPP
#define LEDGER_STORAGE_HPP

#include "bin2hex2bin.hpp"
#include "signatures.hpp"

#include <cryptopp/blake2.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>

#include <stdexcept>
#include <string>


namespace taraxa {

//! Usually Path == boost::filesystem::path
template<class Path> Path get_transaction_path(
	const std::string& previous_hex,
	const Path& transactions_path
) {
	using std::to_string;

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			"Hex format hash of previous transaction must be " + to_string(nr_hash_chars)
			+ " characters but is " + to_string(previous_hex.size())
		);
	}

	auto transaction_path = transactions_path;
	transaction_path /= previous_hex.substr(0, 2);
	transaction_path /= previous_hex.substr(2, 2);
	transaction_path /= previous_hex.substr(4);

	return transaction_path;
}

//! Usually Path == boost::filesystem::path
template<class Path> Path get_account_path(
	const std::string& pubkey_hex,
	const Path& accounts_path
) {
	using std::to_string;

	const auto pubkey_hex_size = 2 * taraxa::public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);
	if (pubkey_hex.size() != pubkey_hex_size) {
		throw std::invalid_argument(
			"Hex format public key must be " + to_string(pubkey_hex_size)
			+ " characters but is " + to_string(pubkey_hex.size())
		);
	}

	auto account_path = accounts_path;
	account_path /= pubkey_hex.substr(0, 2);
	account_path /= pubkey_hex.substr(2);

	return account_path;
}

} // namespace taraxa

#endif // ifndef LEDGER_STORAGE_HPP
