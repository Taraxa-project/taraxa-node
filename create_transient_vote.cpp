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


int main(int argc, char* argv[]) {

	/*
	Parse command line options and validate input
	*/

	std::string exp_hex, candidate_hex, new_balance_str, payload_hex, latest_hex;

	boost::program_options::options_description options(
		"Creates a vote for a transaction. The vote isn't stored permanently\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("latest", boost::program_options::value<std::string>(&latest_hex),
			"Hash of latest transaction from voting account (hex)")
		("candidate", boost::program_options::value<std::string>(&candidate_hex),
			"Hash of transaction that is voted for (hex)")
		("key", boost::program_options::value<std::string>(&exp_hex),
			"Private key used to sign the vote (hex)");

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
		std::cerr << "Hex format private key must be 64 characters but is "
			<< exp_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	const auto nr_hash_chars = 2 * CryptoPP::BLAKE2s::DIGESTSIZE;
	if (latest_hex.size() != nr_hash_chars) {
		std::cerr << "Hex format hash of latest transaction must be " << nr_hash_chars
			<< " characters but is " << latest_hex.size() << std::endl;
		return EXIT_FAILURE;
	}

	if (candidate_hex.size() != nr_hash_chars) {
		std::cerr << "Hex format hash of candidate must be " << nr_hash_chars
			<< " characters but is " << candidate_hex.size() << std::endl;
		return EXIT_FAILURE;
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

	std::cout << vote.to_json_str() << std::endl;

	return EXIT_SUCCESS;
}
