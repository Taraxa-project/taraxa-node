/*
Copyright 2018 Ilja Honkonen
*/


#include "bin2hex2bin.hpp"

#include <boost/program_options.hpp>
#include <cryptopp/aes.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <sodium.h>

#include <cstdlib>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {
	std::string proof_hex;

	boost::program_options::options_description options(
		"Prints VRF output of VRF proof.\n"
		"Input and output must be hex encoded without leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("proof",
		 	boost::program_options::value<std::string>(&proof_hex),
			"VRF proof to use");

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

	if (proof_hex.size() != 2 * crypto_vrf_PROOFBYTES) {
		std::cerr << "Proof must be " << 2 * crypto_vrf_PROOFBYTES << " characters"
			<< " but " << proof_hex.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	const std::string proof_bin = taraxa::hex2bin(proof_hex);
	if (proof_bin.size() != crypto_vrf_PROOFBYTES) {
		std::cerr << "Proof must be " << crypto_vrf_PROOFBYTES << " bytes"
			<< " but " << proof_bin.size() << " given" << std::endl;
		return EXIT_FAILURE;
	}

	std::string output_bin(crypto_vrf_OUTPUTBYTES, ' ');
	if (
		crypto_vrf_proof_to_hash(
			(unsigned char*) output_bin.data(),
			(unsigned char*) proof_bin.data()
		) != 0
	) {
		std::cerr << "Couldn't get output for proof." << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << taraxa::bin2hex(output_bin) << std::endl;

	return EXIT_SUCCESS;
}
