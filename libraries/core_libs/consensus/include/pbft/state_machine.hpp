#pragma once

#include "common/types.hpp"

namespace taraxa {

/**
 * Proposal state: generate PBFT block and propose vote at the block hash

 * Filter state: identify leader block from all received proposed values in the current round by using minimum VRF
 * sortition output. Soft vote at the leader block hash. In filter state, don’t need check voted value correction. This
 * is pre-pre state.

 * Certify state: if receive enough(2t+1) soft vote, cert vote at the value. This is pre state.

 * First finish state: happens at even number steps from step 4. Next vote at finishing value for the current round.
 * This is commit state.

 * Second finish state: happens at odd number steps from step 5. Next vote at finishing value for the current round.
 * This is commit state.
 **/
enum PbftStates : uint8_t { ProposalState = 0, FilterState, CertifyState, FirstFinishState, SecondFinishState };
// enum PbftStates { value_proposal_state = 1, filter_state, certify_state, finish_state, finish_polling_state };

}  // namespace taraxa