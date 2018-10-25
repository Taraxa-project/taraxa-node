/*
Copyright 2018 Ilja Honkonen
*/
#include <cstdlib>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include "create_transient_vote.hpp"

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

	std::string vote = taraxa::create_transient_vote(exp_hex, latest_hex, candidate_hex);
	std::cout << vote << std::endl;

	return EXIT_SUCCESS;
}
