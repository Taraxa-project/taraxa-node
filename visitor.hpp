/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-16 17:07:07 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-17 18:13:18
 */
 
#ifndef VISITOR_HPP
#define VISITOR_HPP

#include <iostream>
#include "types.hpp"
#include "util.hpp"
#include "state_block.hpp"
#include "full_node.hpp"

namespace taraxa{

class BaseVisitor{
public: 
	BaseVisitor(std::shared_ptr<FullNode> full_node);
	virtual void visit(stream & strm) = 0;
	virtual ~BaseVisitor() = default;
	std::shared_ptr<FullNode> full_node_;
};

class BlockVisitor : public BaseVisitor {
public:
	BlockVisitor(std::shared_ptr<FullNode> full_node);
	~BlockVisitor() = default;
	void visit(stream & strm) override;
	// debugging
	StateBlock getBlock();
private:
	StateBlock block_;
};

}
#endif
