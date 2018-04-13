/*
Copyright 2018 Ilja Honkonen
*/

#ifndef ACCOUNTS_HPP
#define ACCOUNTS_HPP

#include "bin2hex2bin.hpp"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>


namespace taraxa {


//! Usually @Hasher == CryptoPP::BLAKE2s
template<class Hasher> class Account {

public:
	std::string
		pubkey_hex,
		address_hex,
		genesis_transaction_hex,
		balance_hex; // TODO: switch to CryptoPP::Integer


void load(const std::string& json_file_name, const bool verbose) {
	try {
		std::ifstream json_file(json_file_name);
		load(json_file, verbose);
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			"Couldn't load account from file " + json_file_name
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
			<< " characters from stdin" << std::endl;
	}

	try {
		load_from_json(json, verbose);
	} catch (const std::exception& e) {
		throw std::invalid_argument(
			std::string("Couldn't read account: ") + e.what()
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
	// normalize hex representation of public key
	pubkey_hex
		= taraxa::bin2hex(
			taraxa::hex2bin(
				std::string(pubkey_json.GetString())
			)
		);
	if (pubkey_hex.size() != 128) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Public key must be 128 characters but is "
			+ to_string(pubkey_hex.size())
		);
	}
	if (verbose) {
		std::cout << "Public key: " << pubkey_hex << std::endl;
	}

	// TODO: compress address
	address_hex = pubkey_hex;
	if (verbose) {
		std::cout << "Address: " << address_hex << std::endl;
	}

	// check for genesis
	if (not document.HasMember("genesis")) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			 "JSON object doesn't have a genesis key"
		);
	}
	const auto& genesis_json = document["genesis"];
	if (not genesis_json.IsString()) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Value of genesis is not a string"
		);
	}
	// normalize hex representation of genesis transaction
	genesis_transaction_hex
		= taraxa::bin2hex(
			taraxa::hex2bin(
				std::string(genesis_json.GetString())
			)
		);
	const auto nr_hash_chars = 2 * Hasher::DIGESTSIZE;
	if (genesis_transaction_hex.size() != nr_hash_chars) {
		throw std::invalid_argument(
			__FILE__ "(" + to_string(__LINE__) + ") "
			"Hash of genesis transaction must be " + to_string(nr_hash_chars)
			+ " characters but is " + to_string(genesis_transaction_hex.size())
		);
	}
	if (verbose) {
		std::cout << "Genesis transaction: " << genesis_transaction_hex << std::endl;
	}
}


rapidjson::Document to_json() const {
	// TODO add error checking
	rapidjson::Document document;
	auto& allocator = document.GetAllocator();
	document.SetObject();
	document.AddMember("public-key", rapidjson::StringRef(pubkey_hex), allocator);
	document.AddMember("genesis", rapidjson::StringRef(genesis_transaction_hex), allocator);
	document.AddMember("balance", rapidjson::StringRef(balance_hex), allocator);
	document.AddMember("address", rapidjson::StringRef(address_hex), allocator);
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

}; // class Account

} // namespace taraxa

#endif // ifndef ACCOUNTS_HPP
