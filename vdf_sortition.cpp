#include "vdf_sortition.hpp"
#include "libdevcore/CommonData.h"
#include "util.hpp"
#include <libdevcrypto/Common.h>
#include <libethcore/Common.h>
namespace taraxa::vdf_sortition {
VdfSortition::VdfSortition(bytes const & b){
  dev::RLP const rlp(b);
  if (!rlp.isList()){
    throw std::invalid_argument("VdfSortition RLP must be a list");
  }
  pk = rlp[0].toHash<vrf_pk_t>();
  proof = rlp[1].toHash<vrf_proof_t>();
  vdf_msg.level = rlp[2].toInt<uint64_t>();
  vdf_msg.last_pbft_hash = rlp[3].toHash<blk_hash_t>(); 
  vdf_sol.first = rlp[4].toBytes();
  vdf_sol.second = rlp[5].toBytes();
  verify();
}
bytes VdfSortition::rlp() const{
  dev::RLPStream s;
  s.appendList(6);
  s << pk;
  s << proof;
  s << vdf_msg.level;
  s << vdf_msg.last_pbft_hash;
  s << vdf_sol.first;
  s << vdf_sol.second;
  return s.out();
}
void VdfSortition::computeVdfSolution() {
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg.toString());
  auto t1 = getCurrentTimeMilliSeconds();
  VerifierWesolowski verifier(lambda, getDifficulty(), msg_bytes, N);

  ProverWesolowski prover;
  vdf_sol = prover(verifier);  // this line takes time ...
  auto t2 = getCurrentTimeMilliSeconds();
  std::cout << "Difficulty is " << getDifficulty() << " , computed in "
            << t2 - t1 << " (ms)" << std::endl;


}
bool VdfSortition::verifyVdfSolution() {
  const auto msg_bytes = vrf_wrapper::getRlpBytes(vdf_msg.toString());
  VerifierWesolowski verifier(lambda, getDifficulty(), msg_bytes, N);
  return verifier(vdf_sol);


}
 
}  // namespace taraxa::vdf_sortition