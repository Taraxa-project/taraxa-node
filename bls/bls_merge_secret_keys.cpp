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

	std::vector<std::string> ids_hex, seckeys_hex;

	boost::program_options::options_description options(
		"Prints merged secret key from given secret keys.\n"
		"Input and output is hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "Print this help message and exit")
		("id",
		 	boost::program_options::value<std::vector<std::string>>(&ids_hex)
				->composing(),
			"Ids of secret keys (as printed by bls_make_threshold_keys)")
		("seckey",
		 	boost::program_options::value<std::vector<std::string>>(&seckeys_hex)
				->composing(),
			"Secret keys to merge in same order as ids");

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

	if (ids_hex.size() != seckeys_hex.size()) {
		std::cerr << "Number of ids and public keys differ." << std::endl;
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

	bls::SecretKeyVec seckeys;
	for (const auto& seckey_hex: seckeys_hex) {
		std::string seckey_bin = taraxa::hex2bin(seckey_hex);
		std::reverse(seckey_bin.begin(), seckey_bin.end());
		bls::SecretKey seckey;
		seckey.setStr(seckey_bin, bls::IoFixedByteSeq);
		seckeys.push_back(seckey);
	}

	bls::SecretKey seckey;
	seckey.recover(seckeys, ids);
	std::string seckey_bin;
	seckey.getStr(seckey_bin, bls::IoFixedByteSeq);
	std::reverse(seckey_bin.begin(), seckey_bin.end());
	std::cout << taraxa::bin2hex(seckey_bin) << std::endl;

	return EXIT_SUCCESS;
}
