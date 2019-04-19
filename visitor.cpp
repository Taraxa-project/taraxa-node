/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-17 14:36:35
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-18 17:19:05
 */

#include "visitor.hpp"

namespace taraxa {
BaseVisitor::BaseVisitor(std::weak_ptr<FullNode> full_node)
    : full_node_(full_node) {}

BlockVisitor::BlockVisitor(std::weak_ptr<FullNode> full_node)
    : BaseVisitor(full_node) {}

void BlockVisitor::visit(stream &strm) {
  block_.deserialize(strm);
  if (auto fn = full_node_.lock()) {
    fn->storeBlock(block_);
  }
}
DagBlock BlockVisitor::getDagBlock() { return block_; }
}  // namespace taraxa
