/*
Copyright 2018 Ilja Honkonen
*/

#ifndef TRANSACTIONS_HPP
#define TRANSACTIONS_HPP

#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "signatures.hpp"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>


namespace taraxa {

//! Usually @Hasher == CryptoPP::BLAKE2s
template<class Hasher> class Transaction {

public:

std::string
	pubkey_hex, // creator's public key
	previous_hex, // hash of previous transaction on creator's chain
	send_hex, // for receive transaction, corresponding send
	receiver_hex, // for send transaction, account that must create receive
	receive_hex, // for send, corresponding receive
	payload_hex,
	hash_hex, // hash of this transaction's data
	new_balance_hex,
	signature_hex, // creator's signature
	next_hex; // hash of next transaction on creator's chain


void load(const std::string& json_file_name, const bool verbose) {
	try {
		if (verbose) {
			std::cout << "Reading from file " << json_file_name
				<< "..." << std::endl;
		}
		std::ifstream json_file(json_file_name);
		load(json_file, verbose);
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			"Couldn't load transaction from file " + json_file_name
			+ ": " + e.what()
		);
	}
}


void load(std::istream& json_file, const bool verbose) {
	std::string json;
	while (json_file.good()) {
		std::string temp;
		std::getline(json_file, temp);
		if (temp.size() > 0) {
			json += temp;
		}
	}

	if (verbose) {
		std::cout << "Read " << json.size() << " characters" << std::endl;
	}

	try {
		load_from_json(json, verbose);
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			std::string("Couldn't read transaction: ") + e.what()
		);
	}
}


void load_from_json(const std::string& json, const bool verbose) {
	using std::to_string;

	rapidjson::Document document;
	document.Parse(json.c_str());
	if (document.HasParseError()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Couldn't parse json data at character position " + to_string(document.GetErrorOffset())
			+ ": " + rapidjson::GetParseError_En(document.GetParseError())
		);
	}

	if (not document.IsObject()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON data doesn't represent an object ( {...} )"
		);
	}

	// check for previous transaction
	if (not document.HasMember("previous")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			 "JSON object doesn't have a previous key"
		);
	}

	const auto& previous_json = document["previous"];
	if (not previous_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of previous is not a string"
		);
	}
	// normalize hex representation of previous transaction
	previous_hex
		= bin2hex(
			hex2bin(
				std::string(previous_json.GetString())
			)
		);
	const auto nr_hash_chars = 2 * Hasher::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format hash of previous transaction must be "
			+ to_string(nr_hash_chars) + " characters but is "
			+ to_string(previous_hex.size())
		);
	}
	if (verbose) {
		std::cout << "Previous transaction: " << previous_hex << std::endl;
	}

	// check for signature and normalize
	if (not document.HasMember("signature")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON object doesn't have a signature key"
		);
	}
	const auto& signature_json = document["signature"];
	if (not signature_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of signature is not a string"
		);
	}
	signature_hex
		= bin2hex(
			hex2bin(
				std::string(signature_json.GetString())
			)
		);
	if (verbose) {
		std::cout << "Signature: " << signature_hex << std::endl;
	}

	// check for public key
	if (not document.HasMember("public-key")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON object doesn't have a public-key key"
		);
	}
	const auto& pubkey_json = document["public-key"];
	if (not pubkey_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of public-key is not a string"
		);
	}
	pubkey_hex
		= bin2hex(
			hex2bin(
				std::string(pubkey_json.GetString())
			)
		);
	const auto pubkey_hex_size = 2 * public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);
	if (pubkey_hex.size() != pubkey_hex_size) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format public key must be " + to_string(pubkey_hex_size)
			+ " characters but is " + to_string(pubkey_hex.size())
		);
	}
	if (verbose) {
		std::cout << "Public key: " << pubkey_hex << std::endl;
	}

	/*
	Verify signature
	*/

	std::string signature_payload_hex;

	// send or receive?
	if (not document.HasMember("send") and not document.HasMember("receiver")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON object doesn't have send or receiver key"
		);
	}
	if (document.HasMember("send") and document.HasMember("receiver")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON object has send and receiver keys"
		);
	}
	// receive
	if (document.HasMember("send")) {

		const auto& send_json = document["send"];
		if (not send_json.IsString()) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Value of send is not string"
			);
		}
		send_hex
			= bin2hex(
				hex2bin(
					std::string(send_json.GetString())
				)
			);
		if (verbose) {
			std::cout << "Send: " << send_hex << std::endl;
		}

		const auto nr_hash_chars = 2 * Hasher::DIGESTSIZE;
		if (send_hex.size() != nr_hash_chars) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Hex format hash of send must be "
				+ to_string(nr_hash_chars) + " characters but is "
				+ to_string(send_hex.size())
			);
		}

	// send
	} else {

		const auto& receiver_json = document["receiver"];
		if (not receiver_json.IsString()) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Value of receiver is not string"
			);
		}
		receiver_hex
			= bin2hex(
				hex2bin(
					std::string(receiver_json.GetString())
				)
			);
		if (verbose) {
			std::cout << "Receiver: " << receiver_hex << std::endl;
		}

		if (document.HasMember("receive")) {
			const auto& receive_json = document["receive"];
			if (not receive_json.IsString()) {
				throw std::invalid_argument(
					__FILE__ "(" + to_string(__LINE__) + ") "
					"Value of receive is not string"
				);
			}
			receive_hex
				= bin2hex(
					hex2bin(
						std::string(receive_json.GetString())
					)
				);
			if (verbose) {
				std::cout << "Receive: " << receive_hex << std::endl;
			}
		}

		if (not document.HasMember("new-balance")) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"JSON object doesn't have new-balance key"
			);
		}
		const auto& new_balance_json = document["new-balance"];
		if (not new_balance_json.IsString()) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Value of new-balance is not string"
			);
		}
		new_balance_hex
			= bin2hex(
				hex2bin(
					std::string(new_balance_json.GetString())
				)
			);
		if (verbose) {
			std::cout << "New balance: " << new_balance_hex << std::endl;
		}

		if (not document.HasMember("payload")) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"JSON object doesn't have payload key"
			);
		}
		const auto& payload_json = document["payload"];
		if (not payload_json.IsString()) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Value of payload is not string"
			);
		}
		payload_hex
			= bin2hex(
				hex2bin(
					std::string(payload_json.GetString())
				)
			);
		if (verbose) {
			std::cout << "Payload: " << payload_hex << std::endl;
		}
	}

	if (document.HasMember("next")) {
		const auto& next_json = document["next"];
		if (not next_json.IsString()) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Value of next is not a string"
			);
		}
		next_hex
			= bin2hex(
				hex2bin(
					std::string(next_json.GetString())
				)
			);
	}
	if (verbose) {
		std::cout << "Next: " << next_hex << std::endl;
	}

	try {
		update_hash();
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Couldn't update transaction hash: " + e.what()
		);
	}
	if (verbose) {
		std::cout << "Hash: " << hash_hex << std::endl;
		std::cout << "Signature OK" << std::endl;
	}

}


rapidjson::Document to_json() const {
	// TODO add error checking
	rapidjson::Document document;
	auto& allocator = document.GetAllocator();
	document.SetObject();
	if (pubkey_hex.size() > 0) {
		document.AddMember("public-key", rapidjson::StringRef(pubkey_hex), allocator);
	}
	if (signature_hex.size() > 0) {
		document.AddMember("signature", rapidjson::StringRef(signature_hex), allocator);
	}
	if (previous_hex.size() > 0) {
		document.AddMember("previous", rapidjson::StringRef(previous_hex), allocator);
	}
	if (receiver_hex.size() > 0) {
		document.AddMember("receiver", rapidjson::StringRef(receiver_hex), allocator);
		document.AddMember("receive", rapidjson::StringRef(receive_hex), allocator);
	}
	document.AddMember("payload", rapidjson::StringRef(payload_hex), allocator);
	if (new_balance_hex.size() > 0) {
		document.AddMember("new-balance", rapidjson::StringRef(new_balance_hex), allocator);
	}
	if (hash_hex.size() > 0) {
		document.AddMember("hash", rapidjson::StringRef(hash_hex), allocator);
	}
	if (send_hex.size() > 0) {
		document.AddMember("send", rapidjson::StringRef(send_hex), allocator);
	}
	if (next_hex.size() > 0) {
		document.AddMember("next", rapidjson::StringRef(next_hex), allocator);
	}
	return document;
}


std::string to_json_str() const {
	const auto document = to_json();
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	return buffer.GetString();
}


void to_json_file(const std::string& file_name) const try {
	const auto json_str = to_json_str();
	std::ofstream json_file(file_name);
	json_file << json_str << std::endl;
} catch (const std::exception& e) {
	throw std::invalid_argument(__func__ + std::string(": ") + e.what());
}


void update_hash() {
	using std::to_string;

	const auto nr_hash_chars = 2 * Hasher::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format hash of previous transaction must be " + to_string(nr_hash_chars)
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

	if (receiver_hex.size() > 0 and receiver_hex.size() != 66) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format receiver must be 66 characters but is "
			+ to_string(receiver_hex.size())
		);
	}

	if (send_hex.size() > 0 and send_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format send must be " + to_string(nr_hash_chars)
			+ " characters but is " + to_string(send_hex.size())
		);
	}

	if (pubkey_hex.size() != 66) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format public key must be 66 characters but is "
			+ to_string(pubkey_hex.size())
		);
	}

	std::string signature_payload_hex;
	if (send_hex.size() > 0) {
		signature_payload_hex = previous_hex + send_hex;
	} else {
		if (new_balance_hex.size() != 16) {
			throw std::invalid_argument(
				__FILE__ "(" + to_string(__LINE__) + ") "
				"Hex format new balance must be 16 characters but is "
				+ to_string(new_balance_hex.size())
			);
		}
		signature_payload_hex
			= previous_hex
			+ new_balance_hex
			+ receiver_hex
			+ payload_hex;
	}

	if (not verify_signature_hex(
		signature_hex,
		signature_payload_hex,
		pubkey_hex
	)) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Signature verification failed"
		);
	}

	hash_hex = get_hash_hex<Hasher>(
		signature_hex + signature_payload_hex
	);
}

}; // class Transaction


//! Usually @Hasher == CryptoPP::BLAKE2s
template<class Hasher> class Transient_Vote {

public:

std::string
	pubkey_hex, // voter's public key
	latest_hex, // hash of latest transaction on voter's chain
	candidate_hex, // hash of transaction that's voted for
	hash_hex, // hash of this vote's data
	signature_hex; // voter's signature


void load(const std::string& json_file_name, const bool verbose) {
	try {
		std::ifstream json_file(json_file_name);
		load(json_file, verbose);
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			"Couldn't load vote from file " + json_file_name
			+ ": " + e.what()
		);
	}
}


void load(std::istream& json_file, const bool verbose) {
	std::string json;
	while (json_file.good()) {
		std::string temp;
		std::getline(json_file, temp);
		if (temp.size() > 0) {
			json += temp;
		}
	}

	if (verbose) {
		std::cout << "Read " << json.size()
			<< " characters from stream" << std::endl;
	}

	try {
		load_from_json(json, verbose);
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			std::string("Couldn't read transaction: ") + e.what()
		);
	}
}


void load_from_json(const std::string& json, const bool verbose) {
	using std::to_string;

	rapidjson::Document document;
	document.Parse(json.c_str());
	if (document.HasParseError()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Couldn't parse json data at character position " + to_string(document.GetErrorOffset())
			+ ": " + rapidjson::GetParseError_En(document.GetParseError())
		);
	}

	if (not document.IsObject()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON data doesn't represent an object ( {...} )"
		);
	}

	// check for latest transaction
	if (not document.HasMember("latest")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			 "JSON object doesn't have a latest key"
		);
	}

	const auto& latest_json = document["latest"];
	if (not latest_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of latest is not a string"
		);
	}
	// normalize hex representation of latest transaction
	latest_hex
		= bin2hex(
			hex2bin(
				std::string(latest_json.GetString())
			)
		);
	const auto nr_hash_chars = 2 * Hasher::DIGESTSIZE;
	if (latest_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format hash of latest transaction must be "
			+ to_string(nr_hash_chars) + " characters but is "
			+ to_string(latest_hex.size())
		);
	}
	if (verbose) {
		std::cout << "Previous transaction: " << latest_hex << std::endl;
	}

	// check for signature and normalize
	if (not document.HasMember("signature")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON object doesn't have a signature key"
		);
	}
	const auto& signature_json = document["signature"];
	if (not signature_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of signature is not a string"
		);
	}
	signature_hex
		= bin2hex(
			hex2bin(
				std::string(signature_json.GetString())
			)
		);
	if (verbose) {
		std::cout << "Signature: " << signature_hex << std::endl;
	}

	// check for public key
	if (not document.HasMember("public-key")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"JSON object doesn't have a public-key key"
		);
	}
	const auto& pubkey_json = document["public-key"];
	if (not pubkey_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of public-key is not a string"
		);
	}
	pubkey_hex
		= bin2hex(
			hex2bin(
				std::string(pubkey_json.GetString())
			)
		);
	const auto pubkey_hex_size = 2 * public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);
	if (pubkey_hex.size() != pubkey_hex_size) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hex format public key must be " + to_string(pubkey_hex_size)
			+ " characters but is " + to_string(pubkey_hex.size())
		);
	}
	if (verbose) {
		std::cout << "Public key: " << pubkey_hex << std::endl;
	}

	const auto& candidate_json = document["candidate"];
	if (not candidate_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of receiver is not string"
		);
	}
	candidate_hex
		= bin2hex(
			hex2bin(
				std::string(candidate_json.GetString())
			)
		);
	if (verbose) {
		std::cout << "Candidate: " << candidate_hex << std::endl;
	}

	/*
	Verify signature
	*/

	try {
		update_hash();
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Couldn't update transaction hash: " + e.what()
		);
	}
	if (verbose) {
		std::cout << "Hash: " << hash_hex << std::endl;
		std::cout << "Signature OK" << std::endl;
	}
}


rapidjson::Document to_json() const {
	// TODO add error checking
	rapidjson::Document document;
	auto& allocator = document.GetAllocator();
	document.SetObject();
	if (pubkey_hex.size() > 0) {
		document.AddMember("public-key", rapidjson::StringRef(pubkey_hex), allocator);
	}
	if (signature_hex.size() > 0) {
		document.AddMember("signature", rapidjson::StringRef(signature_hex), allocator);
	}
	if (latest_hex.size() > 0) {
		document.AddMember("latest", rapidjson::StringRef(latest_hex), allocator);
	}
	if (candidate_hex.size() > 0) {
		document.AddMember("candidate", rapidjson::StringRef(candidate_hex), allocator);
	}
	if (hash_hex.size() > 0) {
		document.AddMember("hash", rapidjson::StringRef(hash_hex), allocator);
	}
	return document;
}


std::string to_json_str() const {
	const auto document = to_json();
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	return buffer.GetString();
}


void to_json_file(const std::string& file_name) const try {
	const auto json_str = to_json_str();
	std::ofstream json_file(file_name);
	json_file << json_str << std::endl;
} catch (const std::exception& e) {
	throw std::invalid_argument(__func__ + std::string(": ") + e.what());
}


void update_hash() {
	using std::to_string;

	// TODO add input checking
	const std::string signature_payload_hex = latest_hex + candidate_hex;

	if (not verify_signature_hex(
		signature_hex,
		signature_payload_hex,
		pubkey_hex
	)) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Signature verification failed"
		);
	}

	hash_hex = get_hash_hex<Hasher>(
		signature_hex + signature_payload_hex
	);
}

}; // class Transient_Vote

} // namespace taraxa

#endif // ifndef TRANSACTIONS_HPP
