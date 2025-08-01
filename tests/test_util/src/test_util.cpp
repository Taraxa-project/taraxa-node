#include "test_util/test_util.hpp"

#include <boost/program_options/variables_map.hpp>
#include <fstream>

#include "app/app.hpp"
#include "common/app_base.hpp"
#include "common/encoding_solidity.hpp"
#include "pbft/pbft_manager.hpp"
#include "plugin/rpc.hpp"
#include "vote_manager/vote_manager.hpp"

namespace taraxa {

auto g_secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd",
                            dev::Secret::ConstructFromStringType::FromHex);

bool wait(const wait_opts& opts, const std::function<void(wait_ctx&)>& poller) {
  struct NullBuffer : std::streambuf {
    int overflow(int c) override { return c; }
  } static null_buf;

  assert(opts.poll_period <= opts.timeout);
  uint64_t attempt_count = opts.timeout / opts.poll_period;
  for (uint64_t i(0); i < attempt_count; ++i) {
    if (i == attempt_count - 1) {
      std::stringstream err_log;
      wait_ctx ctx(i, attempt_count, err_log);
      if (poller(ctx); !ctx.failed()) {
        return true;
      }
      if (const auto& s = err_log.str(); !s.empty()) {
        std::cout << err_log.str() << std::endl;
      }
      return false;
    }
    std::ostream null_strm(&null_buf);
    wait_ctx ctx(i, attempt_count, null_strm);
    if (poller(ctx); !ctx.failed()) {
      return true;
    }
    std::this_thread::sleep_for(opts.poll_period);
  }
  assert(false);
}

void TransactionClient::must_process_sync(const std::shared_ptr<Transaction>& trx) const {
  ASSERT_EQ(process(trx, true).stage, TransactionStage::executed);
}

TransactionClient::Context TransactionClient::process(const std::shared_ptr<Transaction>& trx,
                                                      bool wait_executed) const {
  Context ctx{
      TransactionStage::created,
      trx,
  };
  if (!node_->getTransactionManager()->insertTransaction(ctx.trx).first) {
    return ctx;
  }
  ctx.stage = TransactionStage::inserted;
  auto trx_hash = ctx.trx->getHash();
  if (wait_executed) {
    auto success =
        wait(wait_opts_, [&, this](auto& ctx) { ctx.fail_if(!node_->getFinalChain()->transactionLocation(trx_hash)); });
    if (success) {
      ctx.stage = TransactionStage::executed;
    }
  }
  return ctx;
}

TransactionClient::Context TransactionClient::coinTransfer(const addr_t& to, const val_t& val, bool wait_executed) {
  return process(std::make_shared<Transaction>(nonce_++, val, 0, TEST_TX_GAS_LIMIT, bytes(), secret_, to),
                 wait_executed);
}

SharedTransaction make_dpos_trx(const FullNodeConfig& sender_node_cfg, const u256& value, uint64_t nonce,
                                const u256& gas_price) {
  const auto addr = dev::toAddress(sender_node_cfg.node_secret);
  auto proof = dev::sign(sender_node_cfg.node_secret, dev::sha3(addr)).asBytes();
  // We need this for eth compatibility
  proof[64] += 27;

  const auto vrf_pub_key = vrf_wrapper::getVrfPublicKey(sender_node_cfg.vrf_secret);

  const auto input = util::EncodingSolidity::packFunctionCall(
      "registerValidator(address,bytes,bytes,uint16,string,string)", addr, proof, vrf_pub_key.asBytes(), 10,
      dev::asBytes("test"), dev::asBytes("test"));

  return std::make_shared<Transaction>(nonce, value, gas_price, TEST_TX_GAS_LIMIT, std::move(input),
                                       sender_node_cfg.node_secret, kContractAddress, sender_node_cfg.genesis.chain_id);
}

SharedTransaction make_delegate_tx(const FullNodeConfig& sender_node_cfg, const u256& value, uint64_t nonce,
                                   const u256& gas_price) {
  const auto addr = dev::toAddress(sender_node_cfg.node_secret);
  const auto input = util::EncodingSolidity::packFunctionCall("delegate(address)", addr);
  return std::make_shared<Transaction>(nonce, value, gas_price, TEST_TX_GAS_LIMIT, std::move(input),
                                       sender_node_cfg.node_secret, kContractAddress, sender_node_cfg.genesis.chain_id);
}

SharedTransaction make_undelegate_tx(const FullNodeConfig& sender_node_cfg, const u256& value, uint64_t nonce,
                                     const u256& gas_price) {
  const auto addr = dev::toAddress(sender_node_cfg.node_secret);
  const auto input = util::EncodingSolidity::packFunctionCall("undelegate(address,uint256)", addr, value);
  return std::make_shared<Transaction>(nonce, 0, gas_price, TEST_TX_GAS_LIMIT, std::move(input),
                                       sender_node_cfg.node_secret, kContractAddress, sender_node_cfg.genesis.chain_id);
}

SharedTransaction make_redelegate_tx(const FullNodeConfig& sender_node_cfg, const u256& value, const Address& to,
                                     uint64_t nonce, const u256& gas_price) {
  const auto addr = dev::toAddress(sender_node_cfg.node_secret);
  const auto input = util::EncodingSolidity::packFunctionCall("reDelegate(address,address,uint256)", addr, to, value);
  return std::make_shared<Transaction>(nonce, 0, gas_price, TEST_TX_GAS_LIMIT, std::move(input),
                                       sender_node_cfg.node_secret, kContractAddress, sender_node_cfg.genesis.chain_id);
}

u256 own_balance(const std::shared_ptr<AppBase>& node) {
  return node->getFinalChain()->getBalance(node->getAddress()).first;
}

state_api::BalanceMap effective_initial_balances(const state_api::Config& cfg) {
  state_api::BalanceMap effective_balances = cfg.initial_balances;
  for (const auto& validator_info : cfg.dpos.initial_validators) {
    for (const auto& [delegator, amount] : validator_info.delegations) {
      effective_balances[delegator] -= amount;
    }
  }
  return effective_balances;
}

u256 own_effective_genesis_bal(const FullNodeConfig& cfg) {
  return effective_initial_balances(cfg.genesis.state)[dev::toAddress(dev::Secret(cfg.node_secret))];
}

std::shared_ptr<PbftBlock> make_simple_pbft_block(const h256& hash, uint64_t period, const secret_t& pk) {
  std::vector<vote_hash_t> reward_votes_hashes;
  return std::make_shared<PbftBlock>(hash, kNullBlockHash, kNullBlockHash, kNullBlockHash, period, addr_t(0), pk,
                                     std::move(reward_votes_hashes));
}

std::vector<blk_hash_t> getOrderedDagBlocks(const std::shared_ptr<DbStorage>& db) {
  PbftPeriod period = 1;
  std::vector<blk_hash_t> res;
  while (true) {
    auto pbft_block = db->getPbftBlock(period);
    if (pbft_block.has_value()) {
      for (auto& dag_block_hash : db->getFinalizedDagBlockHashesByPeriod(period)) {
        res.push_back(dag_block_hash);
      }
      period++;
      continue;
    }
    break;
  }
  return res;
}

void wait_for_balances(const std::vector<std::shared_ptr<AppBase>>& nodes, const expected_balances_map_t& balances,
                       wait_opts to_wait) {
  wait(to_wait, [&](auto& ctx) {
    for (const auto& node : nodes) {
      for (const auto& b : balances) {
        if (node->getFinalChain()->getBalance(b.first).first != b.second) {
          auto trx_client = TransactionClient(node);
          trx_client.coinTransfer(KeyPair::create().address(), 0, false);
          WAIT_EXPECT_EQ(ctx, node->getFinalChain()->getBalance(b.first).first, b.second);
        }
      }
      // wait for the same chain size on all nodes
      for (const auto& n : nodes) {
        WAIT_EXPECT_EQ(ctx, node->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks(),
                       n->getPbftChain()->getPbftChainSizeExcludingEmptyPbftBlocks());
      }
    }
  });
}

std::shared_ptr<PbftVote> genDummyVote(PbftVoteTypes type, PbftPeriod period, PbftRound round, PbftStep step,
                                       blk_hash_t block_hash, const std::shared_ptr<VoteManager> vote_mgr) {
  auto vote = vote_mgr->generateVote(block_hash, type, period, round, step);
  vote->calculateWeight(1, 1, 1);
  return vote;
}

std::shared_ptr<PbftVote> genDummyVote(PbftVoteTypes type, PbftPeriod period, PbftRound round, PbftStep step,
                                       blk_hash_t block_hash) {
  VrfPbftMsg msg(type, period, round, step);
  vrf_wrapper::vrf_sk_t vrf_sk(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c"
      "1df1edc9f3367fba550b7971fc2de6c5998d8784051c5be69abc9644");
  VrfPbftSortition vrf_sortition(vrf_sk, msg);
  auto vote = std::make_shared<PbftVote>(g_secret, vrf_sortition, block_hash);
  vote->calculateWeight(1, 1, 1);
  return vote;
}

std::pair<PbftPeriod, PbftRound> clearAllVotes(const std::vector<std::shared_ptr<AppBase>>& nodes) {
  // Get highest round from all nodes
  PbftPeriod max_period = 0;
  PbftRound max_round = 1;
  for (const auto& node : nodes) {
    auto [node_round, node_period] = node->getPbftManager()->getPbftRoundAndPeriod();
    if (node_period > max_period) {
      max_period = node_period;
    }
    if (node_period == max_period && node_round > max_round) {
      max_round = node_round;
    }
  }

  // Clean up votes from memory for each node
  for (const auto& node : nodes) {
    node->getVoteManager()->cleanupVotesByPeriod(max_period + 1);
  }

  return {max_period, max_round};
}

NodesTest::NodesTest() {
  for (uint16_t i = 0; i < 5; ++i) {
    taraxa::FullNodeConfig cfg;

    cfg.data_path = "/tmp/taraxa" + std::to_string(i);
    cfg.db_path = cfg.data_path / "db";
    cfg.log_path = cfg.data_path / "log";
    taraxa::logger::Config log_cfg(cfg.log_path);
    log_cfg.verbosity = taraxa::logger::Verbosity::Error;
    cfg.log_configs.emplace_back(log_cfg);
    cfg.network.rpc.emplace();
    cfg.network.rpc->address = boost::asio::ip::make_address("127.0.0.1");
    cfg.network.rpc->http_port = 7778 + i;
    cfg.network.rpc->ws_port = 8778 + i;
    cfg.node_secret = dev::KeyPair::create().secret();
    cfg.vrf_secret = taraxa::vdf_sortition::getVrfKeyPair().second;
    cfg.network.listen_port = 10003 + i;

    cfg.genesis.gas_price.minimum_price = 0;
    cfg.genesis.state.dpos.yield_percentage = 0;
    cfg.genesis.state.initial_balances[addr_t("de2b1203d72d3549ee2f733b00b2789414c7cea5")] =
        u256(7200999050) * 10000000000000000;  // https://ethereum.stackexchange.com/a/74832

    cfg.network.boot_nodes.clear();
    cfg.network.boot_nodes.emplace_back(
        taraxa::NodeConfig{"7b1fcf0ec1078320117b96e9e9ad9032c06d030cf4024a598347a4623a14a421d4f030cf25ef368ab394a45e9"
                           "20e14b57a259a09c41767dd50d1da27b627412a",
                           "127.0.0.1", 10003});
    cfg.validate();

    node_cfgs.emplace_back(cfg);
  }
  node_cfgs.front().node_secret = dev::Secret("3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd");
  node_cfgs.front().vrf_secret = vdf_sortition::vrf_sk_t(
      "0b6627a6680e01cea3d9f36fa797f7f34e8869c3a526d9ed63ed8170e35542aad05dc12c1df1edc9f3367fba550b7971fc2de6c5998d87"
      "84051c5be69abc9644");
  overwriteFromJsons();
  CleanupDirs();
}

void NodesTest::overwriteFromJsons() {
  auto const DIR = fs::path(__FILE__).parent_path().parent_path();
  auto const DIR_CONF = DIR / "config";
  for (uint8_t i = 0; i < node_cfgs.size(); ++i) {
    auto path = DIR_CONF / (std::string("config") + std::to_string(i) + ".json");
    if (!fs::exists(path)) {
      break;
    }
    Json::Value json;
    std::ifstream(path.string(), std::ifstream::binary) >> json;

    node_cfgs[i].overwriteConfigFromJson(json);
  }
}

void NodesTest::CleanupDirs() {
  for (auto& cfg : node_cfgs) {
    remove_all(cfg.data_path);
  }
}

std::vector<taraxa::FullNodeConfig> NodesTest::make_node_cfgs(size_t total_count, size_t validators_count,
                                                              uint tests_speed, bool enable_rpc_http,
                                                              bool enable_rpc_ws) {
  std::vector<taraxa::FullNodeConfig> ret_configs = node_cfgs;
  assert(total_count <= ret_configs.size());
  assert(validators_count <= total_count);
  ret_configs.erase(ret_configs.begin() + total_count, ret_configs.end());
  assert(ret_configs.size() == total_count);

  if (tests_speed == 1 && enable_rpc_http && enable_rpc_ws) {
    return ret_configs;
  }

  // Prepare genesis balances & initial validators
  taraxa::state_api::BalanceMap initial_balances;
  std::vector<taraxa::state_api::ValidatorInfo> initial_validators;

  // Calculate initial balance based on AspenHf.MaxSupply so "Yield = (MaxSupply - Genesis Balances Sum) / Genesis
  // Balances Sum = 20%

  // Yield [%] = 100 * (max_supply - total_supply) / total_supply
  // Yield * total_supply = 100 * max_supply - 100 * total_supply
  // (Yield * total_supply) + (100 * total_supply) = 100 * max_supply
  // total_supply * (100 + yield) = 100 * max_supply
  // total_supply = 100 * max_supply / (100 + yield)
  // total_supply = num_of_nodes * init_balance
  // num_of_nodes * init_balance = 100 * max_supply / (100 + yield)
  // init_balance = 100 * max_supply / ((100 + yield) * num_of_nodes)

  const taraxa::uint256_t yield{20};  // [%]
  const taraxa::uint256_t hundred{100};
  const taraxa::uint256_t num_of_nodes{total_count};
  const taraxa::uint256_t max_supply = ret_configs.back().genesis.state.hardforks.aspen_hf.max_supply;

  const taraxa::uint256_t init_balance = (hundred * max_supply) / ((hundred + yield) * num_of_nodes);

  for (size_t idx = 0; idx < total_count; idx++) {
    const auto& cfg = ret_configs[idx];
    const auto& node_addr = dev::toAddress(cfg.node_secret);
    initial_balances[node_addr] = init_balance;

    if (idx >= validators_count) {
      continue;
    }

    taraxa::state_api::BalanceMap delegations;
    delegations.emplace(node_addr, cfg.genesis.state.dpos.eligibility_balance_threshold);
    initial_validators.emplace_back(taraxa::state_api::ValidatorInfo{
        node_addr, node_addr, taraxa::vrf_wrapper::getVrfPublicKey(cfg.vrf_secret), 100, "", "", delegations});
  }

  for (size_t idx = 0; idx < total_count; idx++) {
    auto& cfg = ret_configs[idx];

    cfg.genesis.state.initial_balances = initial_balances;
    cfg.genesis.state.dpos.initial_validators = initial_validators;

    cfg.genesis.state.dpos.delegation_delay = 5;
    cfg.genesis.state.dpos.delegation_locking_period = 5;

    // As test are badly written let's disable it for now
    if (tests_speed != 1) {
      // VDF config
      cfg.genesis.sortition.vrf.threshold_upper = 0xffff;
      cfg.genesis.sortition.vdf.difficulty_min = 0;
      cfg.genesis.sortition.vdf.difficulty_max = 5;
      cfg.genesis.sortition.vdf.difficulty_stale = 5;
      cfg.genesis.sortition.vdf.lambda_bound = 100;
      // PBFT config
      cfg.genesis.pbft.lambda_ms /= tests_speed;
      cfg.network.transaction_interval_ms /= tests_speed;
      cfg.network.ddos_protection.vote_accepting_rounds *= tests_speed;
    }
    if (!enable_rpc_http) {
      cfg.network.rpc->http_port = std::nullopt;
    }
    if (!enable_rpc_ws) {
      cfg.network.rpc->ws_port = std::nullopt;
    }
  }

  return ret_configs;
}

bool NodesTest::wait_connect(const std::vector<std::shared_ptr<AppBase>>& nodes) {
  auto num_peers_connected = nodes.size() - 1;
  return wait({60s, 100ms}, [&](auto& ctx) {
    for (const auto& node : nodes) {
      if (ctx.fail_if(node->getNetwork()->getPeerCount() < num_peers_connected)) {
        return;
      }
    }
  });
}

class TestConfig : public cli::Config {
 public:
  TestConfig(const FullNodeConfig& cfg) : cli::Config() {
    node_configured_ = true;
    node_config_ = cfg;
    enableTestRpc();
  }
  void enableTestRpc() {
    cli_options_.insert(std::make_pair("rpc.enable-test-rpc", cli::bpo::variable_value(true, false)));
    cli_options_.insert(std::make_pair("rpc.debug", cli::bpo::variable_value(true, false)));
  }
};

std::shared_ptr<AppBase> NodesTest::create_node(const FullNodeConfig& cfg, bool start) {
  auto app = std::make_shared<App>();

  TestConfig cli_conf(cfg);

  auto rpc_plugin = app->registerPlugin<plugin::Rpc>(cli_conf);

  if (cli_conf.nodeConfigured()) {
    for (const auto& plugin : cli_conf.getEnabledPlugins()) {
      app->enablePlugin(plugin);
    }
    app->init(cli_conf);
  }
  if (start) {
    app->start();
  }
  return app;
}

std::vector<std::shared_ptr<AppBase>> NodesTest::create_nodes(uint count, bool start) {
  auto cfgs = make_node_cfgs(count);
  return create_nodes(cfgs, start);
}

std::vector<std::shared_ptr<AppBase>> NodesTest::create_nodes(const std::vector<FullNodeConfig>& cfgs, bool start) {
  auto node_count = cfgs.size();
  std::vector<std::shared_ptr<AppBase>> nodes;
  for (uint j = 0; j < node_count; ++j) {
    if (j > 0) {
      std::this_thread::sleep_for(500ms);
    }
    nodes.emplace_back(create_node(cfgs[j], start));
  }
  return nodes;
}

std::vector<std::shared_ptr<AppBase>> NodesTest::launch_nodes(const std::vector<taraxa::FullNodeConfig>& cfgs) {
  constexpr auto RETRY_COUNT = 4;
  auto node_count = cfgs.size();
  for (auto i = RETRY_COUNT;; --i) {
    auto nodes = create_nodes(cfgs, true);
    if (node_count == 1) {
      return nodes;
    }
    if (wait_connect(nodes)) {
      std::cout << "Nodes connected and initial status packets passed" << std::endl;
      return nodes;
    }

    if (i == 0) {
      EXPECT_TRUE(false) << "nodes didn't connect properly";
      return nodes;
    }
  }
}

}  // namespace taraxa