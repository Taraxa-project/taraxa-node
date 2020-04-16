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
std::string create_send(
	const std::string &exp_hex, 
	const std::string &receiver_hex, 
	const std::string &new_balance_str, 
	const std::string &payload_hex, 
	const std::string &previous_hex
){
	if (exp_hex.size() != 64) {
		std::cerr << "Key must be 64 characters but is " << exp_hex.size() << std::endl;
		return "";
	}

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (previous_hex.size() != nr_hash_chars) {
		std::cerr << "Hash of previous transaction must be " << nr_hash_chars
			<< " characters but is " << previous_hex.size() << std::endl;
		return "";
	}

	const auto pubkey_hex_size = 2 * taraxa::public_key_size(
		CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey()
	);
	if (receiver_hex.size() != pubkey_hex_size) {
		std::cerr << "Receiver address must be " << pubkey_hex_size
			<< " characters but is " << receiver_hex.size() << std::endl;
		return "";
	}

	const auto keys = taraxa::get_public_key_hex(exp_hex);
	const auto
		exp_bin = taraxa::hex2bin(keys[0]),
		pub_hex = keys[1],
		receiver_bin = taraxa::hex2bin(receiver_hex),
		payload_bin = taraxa::hex2bin(payload_hex),
		previous_bin = taraxa::hex2bin(previous_hex);

	CryptoPP::Integer new_balance(new_balance_str.c_str());
	std::string new_balance_bin(8, 0);
	new_balance.Encode(
		reinterpret_cast<CryptoPP::byte*>(const_cast<char*>(new_balance_bin.data())),
		8
	);

	const auto
		signature_payload_bin = previous_bin + new_balance_bin + receiver_bin + payload_bin,
		//signature is not deterministic
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
	
	const auto new_balance_hex = taraxa::bin2hex(new_balance_bin);

	auto& allocator = document.GetAllocator();
	document.AddMember("signature", rapidjson::StringRef(signature_hex), allocator);
	document.AddMember("previous", rapidjson::StringRef(previous_hex), allocator);
	document.AddMember("receiver", rapidjson::StringRef(receiver_hex), allocator);
	document.AddMember("payload", rapidjson::StringRef(payload_hex), allocator);
	document.AddMember("new-balance", rapidjson::StringRef(new_balance_hex), allocator);
	document.AddMember("public-key", rapidjson::StringRef(pub_hex), allocator);
	document.AddMember("hash", rapidjson::StringRef(hash_hex), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	return buffer.GetString();
}
}
