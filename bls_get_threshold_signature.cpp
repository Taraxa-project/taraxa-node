/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"

#include <bls/bls.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>


int main(int argc, char* argv[]) {

	bls::init();

	std::vector<std::string> ids_hex, signatures_hex;

	boost::program_options::options_description options(
		"Prints merged signature from given partial signatures.\n"
		"Input and output is hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("verbose", "Print PASS or FAIL after verification")
		("id",
		 	boost::program_options::value<std::vector<std::string>>(&ids_hex)
				->composing(),
			"Ids of partial signatures (as printed by bls_make_threshold_keys)")
		("signature",
		 	boost::program_options::value<std::vector<std::string>>(&signatures_hex)
				->composing(),
			"Partial signatures to verify in same order as public keys (as printed by bls_sign_message for each partial private key)");

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

	if (ids_hex.size() != signatures_hex.size()) {
		std::cerr << "Number of ids and signatures differ." << std::endl;
		return EXIT_FAILURE;
	}

	if (ids_hex.size() < 2) {
		std::cerr << "At least two ids required." << std::endl;
		return EXIT_FAILURE;
	}

	bls::IdVec ids;
	for (const auto& id_hex: ids_hex) {
		std::string id_bin = taraxa::hex2bin(id_hex);
		std::reverse(id_bin.begin(), id_bin.end());
		bls::Id id;
		id.setStr(id_bin, bls::IoFixedByteSeq);
		ids.push_back(id);
	}

	bls::SignatureVec signatures;
	for (const auto& signature_hex: signatures_hex) {
		const std::string signature_bin = taraxa::hex2bin(signature_hex);
		bls::Signature signature;
		signature.setStr(signature_bin, bls::IoFixedByteSeq);
		signatures.push_back(signature);
	}

	bls::Signature signature;
	signature.recover(signatures, ids);
	std::string signature_bin;
	signature.getStr(signature_bin, bls::IoFixedByteSeq);
	std::cout << taraxa::bin2hex(signature_bin) << std::endl;

	return EXIT_SUCCESS;
}
