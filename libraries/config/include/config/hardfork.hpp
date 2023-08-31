#pragma once

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"

struct Redelegation {
  taraxa::addr_t validator;
  taraxa::addr_t delegator;
  taraxa::uint256_t amount;
  HAS_RLP_FIELDS
};

Json::Value enc_json(const Redelegation& obj);
void dec_json(const Json::Value& json, Redelegation& obj);

struct Hardforks {
  // disable it by default (set to max uint64)
  uint64_t fix_redelegate_block_num = -1;
  std::vector<Redelegation> redelegations;
  /*
   * @brief key is block number at which change is applied and value is new distribution interval.
   * Default distribution frequency is every block
   * To change rewards distribution frequency we should add a new element in map below.
   * For example {{101, 20}, {201, 10}} means:
   * 1. for blocks [1,100] we are distributing rewards every block
   * 2. for blocks [101, 200] rewards are distributed every 20 block. On blocks 120, 140, etc.
   * 3. for blocks after 201 rewards are distributed every 10 block. On blocks 210, 220, etc.
   */
  using RewardsDistributionMap = std::map<uint64_t, uint32_t>;
  RewardsDistributionMap rewards_distribution_frequency;
  // disable it by default (set to max uint64)
  uint64_t fee_rewards_block_num = -1;
  HAS_RLP_FIELDS
};

Json::Value enc_json(const Hardforks& obj);
void dec_json(const Json::Value& json, Hardforks& obj);