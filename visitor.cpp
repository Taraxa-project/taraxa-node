/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-17 14:36:35 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-17 14:41:17
 */
 
#include "visitor.hpp"

namespace taraxa{
void BlockVisitor::visit(stream &strm){
	block_.deserialize(strm);
}
StateBlock & BlockVisitor::getBlock(){
	return block_;
}
}

