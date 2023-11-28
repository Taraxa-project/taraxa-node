#ifndef TARAXA_NET_RPC_MULTIPROOFS_HPP
#define TARAXA_NET_RPC_MULTIPROOFS_HPP

#include <libdevcore/CommonJS.h>

#include <optional>
#include <string>
#include <unordered_map>

#include "common/types.hpp"
#include "final_chain/state_api_data.hpp"

namespace taraxa::net {

struct KeyValue {
  h256 key;
  bytes value;
  std::vector<uint64_t> indices;
};

struct MultiProofNode {
  bytes proof;
  std::vector<MultiProofNode> sub_proofs;
};

Json::Value toJson(const KeyValue& obj) {
  Json::Value res(Json::objectValue);
  res["key"] = dev::toJS(obj.key);
  Json::Value arr(Json::arrayValue);
  for (const auto& v : obj.indices) {
    arr.append(dev::toJS(v));
  }
  res["indices"] = arr;
  res["value"] = dev::toJS(obj.value);
  return res;
}

Json::Value toJson(const MultiProofNode& obj) {
  Json::Value res(Json::objectValue);
  res["proof"] = dev::toJS(obj.proof);
  Json::Value arr(Json::arrayValue);
  for (const auto& v : obj.sub_proofs) {
    arr.append(toJson(v));
  }
  res["sub_proofs"] = arr;
  return res;
}

struct BytesHash {
  std::size_t operator()(const bytes& b) const { return boost::hash_range(b.cbegin(), b.cend()); }
};

std::pair<MultiProofNode, std::unordered_map<bytes, uint64_t, BytesHash>> build_multiproof_trie(
    const std::vector<taraxa::state_api::StorageProof>& proofs) {
  MultiProofNode root;
  root.proof = proofs[0].proof[0];

  std::unordered_map<bytes, uint64_t, BytesHash> proof_index;

  for (const auto& acc_proof : proofs) {
    MultiProofNode* current = &root;
    for (size_t j = 1; j < acc_proof.proof.size(); ++j) {
      const auto& proof = acc_proof.proof[j];
      bool found = false;
      for (size_t i = 0; i < current->sub_proofs.size(); ++i) {
        if (current->sub_proofs[i].proof == proof) {
          current = &current->sub_proofs[i];
          found = true;
          break;
        }
      }
      if (!found) {
        MultiProofNode new_node;
        new_node.proof = proof;
        current->sub_proofs.push_back(std::move(new_node));
        const size_t index = current->sub_proofs.size() - 1;
        current = &current->sub_proofs[index];
        proof_index[proof] = index;
      }
    }
  }

  return {root, proof_index};
}

std::vector<KeyValue> extract_key_values_with_mapping(
    const taraxa::state_api::ProofResponse& data, const std::unordered_map<bytes, uint64_t, BytesHash>& proof_index) {
  std::vector<KeyValue> ret;
  ret.reserve(data.storage_proof.size());
  for (const auto& proof : data.storage_proof) {
    std::vector<uint64_t> indices;
    indices.reserve(proof.proof.size());
    for (const auto& p : proof.proof) {
      auto it = proof_index.find(p);
      if (it != proof_index.end()) {
        indices.push_back(it->second);
      }
    }
    ret.emplace_back(proof.key, proof.value, indices);
  }
  return ret;
}

std::pair<std::vector<KeyValue>, MultiProofNode> generate_multiproof(const taraxa::state_api::ProofResponse& data) {
  auto [trie, indices] = build_multiproof_trie(data.storage_proof);
  auto accounts_indexes = extract_key_values_with_mapping(data, indices);
  return {accounts_indexes, trie};
}

}  // namespace taraxa::net

#endif  // TARAXA_NET_RPC_MULTIPROOFS_HPP