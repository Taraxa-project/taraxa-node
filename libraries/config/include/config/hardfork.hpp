#pragma once

#include <libdevcore/CommonJS.h>

#include "common/encoding_rlp.hpp"
#include "common/types.hpp"

namespace taraxa {
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

struct AspenHardfork {
  // Part 1 prepares db data that are required for part 2 to be functional
  uint64_t block_num_part_one{0};
  // Part 2 implements new yield curve
  uint64_t block_num_part_two{0};

  taraxa::uint256_t max_supply{"0x26C62AD77DC602DAE0000000"};  // 12 Billion
  // total generated rewards from block 1 to block_num
  // It is partially estimated for blocks between the aspen hf release block and actual aspen hf block_num
  taraxa::uint256_t generated_rewards{0};

  HAS_RLP_FIELDS
};
Json::Value enc_json(const AspenHardfork& obj);
void dec_json(const Json::Value& json, AspenHardfork& obj);

struct FicusHardforkConfig {
  uint64_t block_num{10};
  uint64_t pillar_blocks_interval{10};     // [periods] how often is the new pillar block created
  taraxa::addr_t bridge_contract_address;  // [address] of the bridge contract

  bool isFicusHardfork(taraxa::PbftPeriod period) const;

  /**
   * @param period
   * @param skip_first_pillar_block if true, isPillarBlockPeriod returns false if period == first pillar block period
   * @return true if period is the pbft period, during which new pillar block is created
   */
  bool isPillarBlockPeriod(taraxa::PbftPeriod period, bool skip_first_pillar_block = false) const;

  /**
   * @param period
   * @return true if period is the period, during which pillar block hash is included in pbft block
   */
  bool isPbftWithPillarBlockPeriod(taraxa::PbftPeriod period) const;

  /**
   * @return first pillar block period
   */
  taraxa::PbftPeriod firstPillarBlockPeriod() const;

  void validate(uint32_t delegation_delay) const;

  HAS_RLP_FIELDS
};
Json::Value enc_json(const FicusHardforkConfig& obj);
void dec_json(const Json::Value& json, FicusHardforkConfig& obj);

// Keeping it for next HF
// struct BambooRedelegation {
//   taraxa::addr_t validator;
//   taraxa::uint256_t amount;
//   HAS_RLP_FIELDS
// };
// Json::Value enc_json(const BambooRedelegation& obj);
// void dec_json(const Json::Value& json, BambooRedelegation& obj);

// struct BambooHardfork {
//   uint64_t block_num{0};
//   std::vector<BambooRedelegation> redelegations;

//   HAS_RLP_FIELDS
// };
// Json::Value enc_json(const BambooHardfork& obj);
// void dec_json(const Json::Value& json, BambooHardfork& obj);

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

  // disable it by default (set to max uint64)
  uint64_t phalaenopsis_hf_block_num = -1;
  // disable it by default (set to max uint64)
  uint64_t fix_claim_all_block_num = -1;

  // Aspen hardfork implements new yield curve
  AspenHardfork aspen_hf;

  bool isAspenHardforkPartOne(uint64_t block_number) const { return block_number >= aspen_hf.block_num_part_one; }

  // Ficus hardfork: implementation of pillar chain
  FicusHardforkConfig ficus_hf;

  HAS_RLP_FIELDS
};

Json::Value enc_json(const HardforksConfig& obj);
void dec_json(const Json::Value& json, HardforksConfig& obj);
}  // namespace taraxa
