#pragma once

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"

struct Hardforks {
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

  // Magnolia hardfork:
  // 1.fixing premature deletion of validators in dpos contract -> validator is deleted only
  //  after last delegator confirms his undelegation and:
  //  total_stake == 0, rewards_pool == 0, undelegations_count == 0.
  // 2. changing fee rewards distribution.
  //  Rewards will be distributed to dag blocks creator commission pool, but not directly to the balance of pbft block
  //  creator.
  uint64_t magnolia_hf_block_num = -1;

  HAS_RLP_FIELDS
};

Json::Value enc_json(const Hardforks& obj);
void dec_json(const Json::Value& json, Hardforks& obj);