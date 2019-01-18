/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-16 17:07:07 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-17 14:40:33
 */
 
#ifndef VISITOR_HPP
#define VISITOR_HPP

#include <iostream>
#include "types.hpp"
#include "util.hpp"
#include "state_block.hpp"
namespace taraxa{

class BaseVisitor{
public: 
	virtual void visit(stream & strm) = 0;
	virtual ~BaseVisitor();
};

class BlockVisitor : public BaseVisitor {
public:
	void visit(stream & strm) override;
	StateBlock & getBlock();
private:
	StateBlock block_;
};

}
#endif
