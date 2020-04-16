/*
Copyright 2018 Ilja Honkonen
*/

#define RAPIDJSON_HAS_STDSTRING 1

#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "signatures.hpp"
#include "transactions.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/blake2.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace taraxa{
std::string create_transient_vote(
	const std::string & exp_hex, 
	const std::string & latest_hex,
	const std::string & candidate_hex
){
	std::string new_balance_str, payload_hex;

	if (exp_hex.size() != 64) {
		std::cerr << "Hex format private key must be 64 characters but is "
			<< exp_hex.size() << std::endl;
		return "";
	}

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (latest_hex.size() != nr_hash_chars) {
		std::cerr << "Hex format hash of latest transaction must be " << nr_hash_chars
			<< " characters but is " << latest_hex.size() << std::endl;
		return "";
	}

	if (candidate_hex.size() != nr_hash_chars) {
		std::cerr << "Hex format hash of candidate must be " << nr_hash_chars
			<< " characters but is " << candidate_hex.size() << std::endl;
		return "";
	}


	/*
	Convert input to binary, sign and hash
	*/

	const auto keys = taraxa::get_public_key_hex(exp_hex);
	taraxa::Transient_Vote<CryptoPP::BLAKE2s> vote;
	vote.pubkey_hex = keys[1];
	vote.latest_hex = latest_hex;
	vote.candidate_hex = candidate_hex;
	vote.signature_hex = taraxa::sign_message_hex(latest_hex + candidate_hex, keys[0]);
	vote.update_hash();

	return vote.to_json_str();
}
}
