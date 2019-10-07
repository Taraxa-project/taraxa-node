/*
Copyright 2018 Ilja Honkonen
*/

/*
Header declares all block creation API
*/

#ifndef CREATE_TRANSIENT_VOTE_HPP
#define CREATE_TRANSIENT_VOTE_HPP
#include <string>

namespace taraxa{
std::string create_transient_vote(
	const std::string & exp_hex, 
	const std::string & latest_hex,
	const std::string & candidate_hex
);
}
#endif
