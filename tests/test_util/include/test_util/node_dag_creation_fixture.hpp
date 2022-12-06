#include "../../gtest.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block_proposer.hpp"
#include "dag/dag_manager.hpp"
#include "pbft/pbft_manager.hpp"
#include "samples.hpp"

namespace taraxa::core_tests {

struct NodeDagCreationFixture : NodesTest {
  uint64_t nonce = 1;
  uint64_t dummy_nonce = 0;
  std::shared_ptr<FullNode> node;
  dev::KeyPair dummy = dev::KeyPair::create();
  std::optional<addr_t> contract_addr;

  NodeDagCreationFixture() : NodesTest() {}
  ~NodeDagCreationFixture() = default;
  struct DagBlockWithTxs {
    DagBlock blk;
    SharedTransactions trxs;
  };
  void modifyConfig(FullNodeConfig &cfg);
  void makeNode(bool start = true);
  void makeNodeFromConfig(std::vector<FullNodeConfig> cfgs, bool start = true);

  uint32_t getInitialDagSize();

  void dummyTransaction();

  void deployContract();

  uint64_t trxEstimation();

  SharedTransactions makeTransactions(uint32_t count);
  void insertBlocks(std::vector<DagBlockWithTxs> blks_with_txs);

  void insertTransactions(SharedTransactions transactions);

  void generateAndApplyInitialDag();

  std::vector<DagBlockWithTxs> generateDagBlocks(uint16_t levels, uint16_t blocks_per_level, uint16_t trx_per_block);
};

}  // namespace taraxa::core_tests