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

struct MagnoliaHardfork {
  uint64_t block_num = -1;
  uint64_t jail_time = 0;  // number of blocks

  HAS_RLP_FIELDS
};
Json::Value enc_json(const MagnoliaHardfork& obj);
void dec_json(const Json::Value& json, MagnoliaHardfork& obj);

struct FicusHardforkConfig {
  uint64_t block_num{0};
  uint64_t pillar_block_periods{100};     // [periods] how often is the new pillar block created
  uint64_t signatures_check_periods{25};  // [periods] how often is 2t+1 bls signatures for latest pillar block checked

  HAS_RLP_FIELDS
};
Json::Value enc_json(const FicusHardforkConfig& obj);
void dec_json(const Json::Value& json, FicusHardforkConfig& obj);

struct HardforksConfig {
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

  // Magnolia hardfork:
  // 1.fixing premature deletion of validators in dpos contract -> validator is deleted only
  //  after last delegator confirms his undelegation and:
  //  total_stake == 0, rewards_pool == 0, undelegations_count == 0.
  // 2. changing fee rewards distribution.
  //  Rewards will be distributed to dag blocks creator commission pool, but not directly to the balance of pbft block
  //  creator.
  // 3. Introducing slashing/jailing contract - in case someone double votes - he is jailed for N blocks and unable to
  //    participate in consensus
  MagnoliaHardfork magnolia_hf;

  // Ficus hardfork: implementation of pillar block & bls signatures required for bridge
  FicusHardforkConfig ficus_hf;

  HAS_RLP_FIELDS
};

Json::Value enc_json(const HardforksConfig& obj);
void dec_json(const Json::Value& json, HardforksConfig& obj);
