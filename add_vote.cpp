/*
Copyright 2018 Ilja Honkonen
*/

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define RAPIDJSON_HAS_STDSTRING 1

#include "accounts.hpp"
#include "bin2hex2bin.hpp"
#include "hashes.hpp"
#include "ledger_storage.hpp"
#include "signatures.hpp"
#include "transactions.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {

	/*
	Parse command line options and validate input
	*/

	bool verbose = false;
	std::string ledger_path_str;

	boost::program_options::options_description options(
		"Reads a vote from standard input and adds it to the ledger.\n"
		"All hex encoded strings must be given without the leading 0x.\n"
		"Usage: program_name [options], where options are:"
	);
	options.add_options()
		("help", "print this help message and exit")
		("verbose", "Print more information during execution")
		("ledger-path",
		 	boost::program_options::value<std::string>(&ledger_path_str),
			"Ledger data is located in directory arg");

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

	if (option_variables.count("verbose") > 0) {
		verbose = true;
	}

	/*
	Prepare ledger directory
	*/

	boost::filesystem::path ledger_path(ledger_path_str);

	if (ledger_path.size() > 0) {
		if (verbose and not boost::filesystem::portable_directory_name(ledger_path_str)) {
			std::cout << "WARNING: ledger-path isn't portable." << std::endl;
		}

		if (not boost::filesystem::exists(ledger_path)) {
			if (verbose) {
				std::cout << "Ledger directory doesn't exist, creating..." << std::endl;
			}

			try {
				boost::filesystem::create_directories(ledger_path);
			} catch (const std::exception& e) {
				std::cerr << "Couldn't create ledger directory '" << ledger_path
					<< "': " << e.what() << std::endl;
				return EXIT_FAILURE;
			}
		}

		if (not boost::filesystem::is_directory(ledger_path)) {
			std::cerr << "Ledger path isn't a directory: " << ledger_path << std::endl;
			return EXIT_FAILURE;
		}
	}

	// create subdirectory for vote data
	auto votes_path = ledger_path;
	votes_path /= "votes";
	if (not boost::filesystem::exists(votes_path)) {
		try {
			if (verbose) {
				std::cout << "Votes directory doesn't exist, creating..." << std::endl;
			}
			boost::filesystem::create_directory(votes_path);
		} catch (const std::exception& e) {
			std::cerr << "Couldn't create a directory for votes: " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

	/*
	Read and verify input
	*/

	taraxa::Transient_Vote<CryptoPP::BLAKE2s> vote;
	try {
		// TODO verify input
		vote.load(std::cin, verbose);
	} catch (const std::exception& e) {
		std::cerr << "Couldn't load vote from stdin: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	/*
	Add vote to ledger data
	*/

	const auto
		vote_path = taraxa::get_vote_path(vote.hash_hex, votes_path),
		vote_dir = vote_path.parent_path();

	if (boost::filesystem::exists(vote_path)) {
		if (verbose) {
			std::cerr << "Vote already exists." << std::endl;
		}
		return EXIT_SUCCESS;
	}
	if (not boost::filesystem::exists(vote_dir)) {
		if (verbose) {
			std::cout << "Vote directory doesn't exist, creating..." << std::endl;
		}
		boost::filesystem::create_directories(vote_dir);
	}
	if (verbose) {
		std::cout << "Writing vote to " << vote_path << std::endl;
	}

	vote.to_json_file(vote_path.string());

	return EXIT_SUCCESS;
}
