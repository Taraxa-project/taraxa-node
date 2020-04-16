/*
Copyright 2018 Ilja Honkonen
*/

#define RAPIDJSON_HAS_STDSTRING 1

#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "signatures.hpp"

#include <cryptopp/blake2.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace taraxa{

std::string create_receive(
	const std::string & exp_hex, 
	const std::string & previous_hex,
	const std::string & send_hex
){
	if (exp_hex.size() != 64) {
		std::cerr << "Hex format private key must be 64 characters but is " << exp_hex.size() << std::endl;
		return "";
	}

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		std::cerr << "Hex format hash of previous transaction must be " << nr_hash_chars
			<< " characters but is " << previous_hex.size() << std::endl;
		return "";
	}

	if (send_hex.size() != nr_hash_chars) {
		std::cerr << "Hex format hash of send must be " << nr_hash_chars
			<< " characters but is " << send_hex.size() << std::endl;
		return "";
	}

	/*
	Convert input to binary, sign and hash
	*/

	const auto keys = taraxa::get_public_key_hex(exp_hex);
	const auto
		exp_bin = taraxa::hex2bin(keys[0]),
		pub_hex = keys[1],

		send_bin = taraxa::hex2bin(send_hex),
		previous_bin = taraxa::hex2bin(previous_hex),
		signature_payload_bin = previous_bin + send_bin,
		signature_bin = taraxa::sign_message_bin(signature_payload_bin, exp_bin),

		hash_payload_bin = signature_bin + signature_payload_bin,
		signature_hex = taraxa::bin2hex(signature_bin),
		// hash added only to make users' life easier
		hash_hex = taraxa::bin2hex(taraxa::get_hash_bin<CryptoPP::BLAKE2s>(hash_payload_bin));

	/*
	Convert send to JSON and print to stdout
	*/

	rapidjson::Document document;
	document.SetObject();

	// output what was signed

	auto& allocator = document.GetAllocator();
	document.AddMember("signature", rapidjson::StringRef(signature_hex), allocator);
	document.AddMember("previous", rapidjson::StringRef(previous_hex), allocator);
	document.AddMember("send", rapidjson::StringRef(send_hex), allocator);
	document.AddMember("public-key", rapidjson::StringRef(pub_hex), allocator);
	document.AddMember("hash", rapidjson::StringRef(hash_hex), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	return buffer.GetString();
}
}
