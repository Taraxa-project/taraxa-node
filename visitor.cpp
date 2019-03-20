/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-17 14:36:35
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:19:05
 */

#include "visitor.hpp"
#include "full_node.hpp"

namespace taraxa {
BaseVisitor::BaseVisitor(std::shared_ptr<FullNode> full_node)
    : full_node_(full_node) {}

BlockVisitor::BlockVisitor(std::shared_ptr<FullNode> full_node)
    : BaseVisitor(full_node) {}

void BlockVisitor::visit(stream &strm) {
  block_.deserialize(strm);
  full_node_->storeBlock(block_);
}
DagBlock BlockVisitor::getBlock() { return block_; }
}  // namespace taraxa
