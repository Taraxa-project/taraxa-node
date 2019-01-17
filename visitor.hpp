/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-16 17:07:07 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-16 17:15:46
 */
 
#ifndef VISITOR_HPP
#define VISITOR_HPP

#include <iostream>

class BaseVisitor{
public: 
	virtual void visit() = 0;
	virtual ~BaseVisitor();
};

class BlockVisitor : public BaseVisitor {
public:

};

#endif