/*
Copyright 2018 Ilja Honkonen
*/

#ifndef TRANSACTIONS_HPP
#define TRANSACTIONS_HPP

#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "signatures.hpp"

#include <cryptopp/integer.h>

#include <stdexcept>
#include <string>


namespace taraxa {

//! Usually @Hasher == CryptoPP::BLAKE2s and @Chars == std::string
template<
	class Hasher,
	class Chars
> class Transaction {

public:

std::string
	pubkey_hex, // creator's public key
	previous_hex, // hash of previous transaction on creator's chain
	send_hex, // corresponding send for a receive
	receiver_hex, // account that must create corresponding receive
	payload_hex,
	hash_hex, // hash of this transaction's data
	new_balance_hex,
	signature_hex;
CryptoPP::Integer new_balance;


void update_hash() {
	using std::to_string;

	const auto nr_hash_chars = 2 * Hasher::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hash of previous transaction must be " + to_string(nr_hash_chars)
			+ " characters but is " + to_string(previous_hex.size())
		);
	}

	if (send_hex.size() > 0 and receiver_hex.size() > 0) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Transaction cannot be both send and receive: "
			+ send_hex + ", " + receiver_hex
		);
	}
	if (send_hex.size() == 0 and receiver_hex.size() == 0) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Transaction must be either send or receive"
		);
	}

	if (receiver_hex.size() > 0 and receiver_hex.size() != 128) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Receiver must be 128 characters but is "
			+ to_string(receiver_hex.size())
		);
	}

	if (send_hex.size() > 0 and send_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Send must be " + to_string(nr_hash_chars)
			+ " characters but is " + to_string(send_hex.size())
		);
	}

	if (pubkey_hex.size() != 128) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Public key must be 128 characters but is "
			+ to_string(pubkey_hex.size())
		);
	}

	Chars signature_payload_hex;
	if (send_hex.size() > 0) {
		signature_payload_hex = previous_hex + send_hex;
	} else {
		if (new_balance_hex.size() != 16) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"New balance must be 16 characters but is "
				+ to_string(new_balance_hex.size())
			);
		}
		signature_payload_hex
			= previous_hex
			+ new_balance_hex
			+ receiver_hex
			+ payload_hex;
	}

	if (not taraxa::verify_signature_hex(
		signature_hex,
		signature_payload_hex,
		pubkey_hex.substr(0, pubkey_hex.size() / 2),
		pubkey_hex.substr(pubkey_hex.size() / 2, pubkey_hex.size() / 2)
	)) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Signature verification failed"
		);
	}

	hash_hex = taraxa::get_hash_hex<Hasher>(
		signature_hex + signature_payload_hex
	);
}

};

} // namespace taraxa

#endif // ifndef TRANSACTIONS_HPP
