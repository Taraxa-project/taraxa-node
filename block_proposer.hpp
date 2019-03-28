/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-24 11:30:32
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-25 17:15:20
 */

#ifndef BLOCK_PROPOSER_HPP
#define BLOCK_PROPOSER_HPP

#include <vector>
#include "boost/thread.hpp"
namespace taraxa {

class DagManager;

class BlockProposer {
 public:
  BlockProposer(unsigned num_threads, std::shared_ptr<DagManager> dag_mgr);
  ~BlockProposer();
  void proposeBlock();
  void start();
  void stop();

  // debug
  void setVerbose(bool verbose);
  static uint64_t getNumProposedBlocks();

 private:
  static std::atomic<uint64_t> num_proposed_blocks;
  bool verbose_ = false;
  bool stopped_ = true;
  unsigned num_threads_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::vector<boost::thread> proposer_threads_;
};

}  // namespace taraxa

#endif
