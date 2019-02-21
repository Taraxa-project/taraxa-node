/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-01-28 11:12:11 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-20 23:09:40
 */
 
#include <gtest/gtest.h>
#include "dag.hpp"
#include "types.hpp"
namespace taraxa{
TEST(Dag, build_dag){  
	taraxa::Dag graph;

	// a genesis vertex
	EXPECT_EQ(1, graph.getNumVertices()); 

	auto v1="0000000000000000000000000000000000000000000000000000000000000001";
	auto v2="0000000000000000000000000000000000000000000000000000000000000002";
	auto v3="0000000000000000000000000000000000000000000000000000000000000003";
	
	std::vector<std::string> empty;
	graph.addVEEs(v1, Dag::GENESIS, empty);
	EXPECT_EQ(2, graph.getNumVertices());
	EXPECT_EQ(1, graph.getNumEdges());

	// try insert same vertex, no multiple edges
	graph.addVEEs(v1, Dag::GENESIS, empty);
	EXPECT_EQ(2, graph.getNumVertices());
	EXPECT_EQ(1, graph.getNumEdges());

	graph.addVEEs(v2, Dag::GENESIS, empty);
	EXPECT_EQ(3, graph.getNumVertices());
	EXPECT_EQ(2, graph.getNumEdges());

	graph.addVEEs(v3, v1, {v2});
	EXPECT_EQ(4, graph.getNumVertices());
	EXPECT_EQ(4, graph.getNumEdges());

}

TEST(Dag, dag_traverse){  
	taraxa::Dag graph;

	// a genesis vertex
	EXPECT_EQ(1, graph.getNumVertices()); 

	auto v1="0000000000000000000000000000000000000000000000000000000000000001";
	auto v2="0000000000000000000000000000000000000000000000000000000000000002";
	auto v3="0000000000000000000000000000000000000000000000000000000000000003";
	auto v4="0000000000000000000000000000000000000000000000000000000000000004";
	auto v5="0000000000000000000000000000000000000000000000000000000000000005";
	auto v6="0000000000000000000000000000000000000000000000000000000000000006";
	auto v7="0000000000000000000000000000000000000000000000000000000000000007";
	auto v8="0000000000000000000000000000000000000000000000000000000000000008";
	auto v9="0000000000000000000000000000000000000000000000000000000000000009";

	std::vector<std::string> empty;
	std::string no="";
	// isolate node
	graph.addVEEs(v1, no, empty);
	graph.addVEEs(v2, no, empty);
	EXPECT_EQ(3, graph.getNumVertices());
	EXPECT_EQ(0, graph.getNumEdges());

	std::vector<std::string> leaves;
	std::string pivot; 
	graph.collectTips(leaves);
	EXPECT_EQ(3, leaves.size());
	
	graph.addVEEs(v3, Dag::GENESIS, empty);
	graph.addVEEs(v4, Dag::GENESIS, empty);
	EXPECT_EQ(5, graph.getNumVertices());
	EXPECT_EQ(2, graph.getNumEdges());

	graph.addVEEs(v5, v3, empty);
	graph.addVEEs(v6, v3, empty);
	graph.addVEEs(v7, v3, empty);
	EXPECT_EQ(8, graph.getNumVertices());
	EXPECT_EQ(5, graph.getNumEdges());

	graph.addVEEs(v8, v6, {v5});
	graph.addVEEs(v9, v6, empty);
	leaves.clear();
	graph.collectTips(leaves);
	EXPECT_EQ(6, leaves.size());
	graph.collectPivot(pivot);
	EXPECT_EQ(v8, pivot);

	time_stamp_t t7 = graph.getVertexTimeStamp(v7);
	time_stamp_t t7p1 = t7+1;

	std::vector<std::string> children, tips;
	graph.getChildrenBeforeTimeStamp(v3, t7, children);
	EXPECT_EQ(children.size(), 2);

	graph.getChildrenBeforeTimeStamp(v6, t7, children);
	EXPECT_EQ(children.size(), 0);

	graph.getTipsBeforeTimeStamp(v3, t7p1, tips);
	EXPECT_EQ(tips.size(), 3);

	graph.getTipsBeforeTimeStamp(v3, t7, tips);
	EXPECT_EQ(tips.size(), 2);

	graph.getTipsBeforeTimeStamp(v4, t7p1, tips);
	EXPECT_EQ(tips.size(), 1);

	time_stamp_t t4 = graph.getVertexTimeStamp(v4);
	time_stamp_t t4p1 = t4+1;
	time_stamp_t t8 = graph.getVertexTimeStamp(v8);
	time_stamp_t t8p1 = t8+1;
	{
		std::vector<std::string> pivot_chain;
		graph.getPivotChainBeforeTimeStamp(Dag::GENESIS, t4p1, pivot_chain);
		EXPECT_EQ(pivot_chain.size(), 1);
		EXPECT_EQ(pivot_chain.back(), v3);
	}

	{
		std::vector<std::string> pivot_chain;
		graph.getPivotChainBeforeTimeStamp(Dag::GENESIS, t7p1, pivot_chain);
		EXPECT_EQ(pivot_chain.size(), 2);
		EXPECT_EQ(pivot_chain.back(), v5);
	}
	{
		std::vector<std::string> pivot_chain;	
		graph.getPivotChainBeforeTimeStamp(Dag::GENESIS, t8p1, pivot_chain);
		EXPECT_EQ(pivot_chain.size(), 3);
		EXPECT_EQ(pivot_chain.back(), v8);
	}
	{
		std::vector<std::string> pivot_chain;	
		graph.getPivotChainBeforeTimeStamp(Dag::GENESIS, t8p1, pivot_chain);
		EXPECT_EQ(pivot_chain.size(), 3);
		EXPECT_EQ(pivot_chain.back(), v8);
	}
	{
		std::vector<std::string> pivot_chain;
		graph.getPivotChainBeforeTimeStamp(v3, t8p1, pivot_chain);
		EXPECT_EQ(pivot_chain.size(), 2);
		EXPECT_EQ(pivot_chain.back(), v8);
	}

}

TEST(Dag, dag_traverse2){  
	taraxa::Dag graph;

	// a genesis vertex
	EXPECT_EQ(1, graph.getNumVertices()); 

	auto v1="0000000000000000000000000000000000000000000000000000000000000001";
	auto v2="0000000000000000000000000000000000000000000000000000000000000002";
	auto v3="0000000000000000000000000000000000000000000000000000000000000003";
	auto v4="0000000000000000000000000000000000000000000000000000000000000004";
	auto v5="0000000000000000000000000000000000000000000000000000000000000005";
	auto v6="0000000000000000000000000000000000000000000000000000000000000006";

	std::vector<std::string> empty;
	std::string no="";
	
	
	graph.addVEEs(v1, Dag::GENESIS, empty);
	graph.addVEEs(v2, v1, empty);
	graph.addVEEs(v3, v2, empty);
	graph.addVEEs(v4, v2, empty);
	graph.addVEEs(v5, v2, empty);
	graph.addVEEs(v6, v4, {v5});

	EXPECT_EQ(7, graph.getNumVertices());
	EXPECT_EQ(7, graph.getNumEdges());

	time_stamp_t t5 = graph.getVertexTimeStamp(v5);
	time_stamp_t t5p1 = t5+1;
	time_stamp_t t6 = graph.getVertexTimeStamp(v6);
	time_stamp_t t6p1 = t6+1;
	std::vector<std::string> children, tips;
	graph.getChildrenBeforeTimeStamp(v2, t5, children);
	EXPECT_EQ(children.size(), 2);

	graph.getChildrenBeforeTimeStamp(v2, t5p1, children);
	EXPECT_EQ(children.size(), 3);

	graph.getTipsBeforeTimeStamp(v4, t6p1, tips);
	EXPECT_EQ(tips.size(), 1);

	graph.getTipsBeforeTimeStamp(v1, t5, tips);
	EXPECT_EQ(tips.size(), 2);

	graph.getTipsBeforeTimeStamp(v1, t6p1, tips);
	EXPECT_EQ(tips.size(), 2);

	graph.getTipsBeforeTimeStamp(v4, t5, tips);
	EXPECT_EQ(tips.size(), 1);

	time_stamp_t stamp = 100;
	graph.setVertexTimeStamp(v1, stamp);
	EXPECT_EQ(graph.getVertexTimeStamp(v1), stamp);
	

}


TEST(DagManager, receive_block_in_order){
	auto mgr = std::make_shared<DagManager> (1);
	mgr->start();
	// mgr.setVerbose(true);
	StateBlock blk1 (
	"0000000000000000000000000000000000000000000000000000000000000000",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000001",
	"000000000000000000000000000000000000000000000000000000000000000F");

	StateBlock blk2 (
	"0000000000000000000000000000000000000000000000000000000000000001",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000002",
	"000000000000000000000000000000000000000000000000000000000000000F");
	
	StateBlock blk3 (
	"0000000000000000000000000000000000000000000000000000000000000000",
	{"0000000000000000000000000000000000000000000000000000000000000001",
	 "0000000000000000000000000000000000000000000000000000000000000002"
	}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000003",
	"000000000000000000000000000000000000000000000000000000000000000F");

	mgr->addStateBlock(blk1, true);
	mgr->addStateBlock(blk2, true);
	mgr->addStateBlock(blk2, true);
	EXPECT_EQ(mgr->getNumVerticesInDag(),3);
	EXPECT_EQ(mgr->getNumEdgesInDag(),2);

	mgr->addStateBlock(blk3, true);
	mgr->addStateBlock(blk3, true);
	taraxa::thisThreadSleepForMilliSeconds(500);
	mgr->stop();
	EXPECT_EQ(mgr->getNumVerticesInDag(),4);
	EXPECT_EQ(mgr->getNumEdgesInDag(),5);
	EXPECT_EQ(mgr->getBufferSize(), 0);

}

TEST(DagManager, receive_block_out_of_order){
	auto mgr = std::make_shared<DagManager> (1);
	mgr->start();

	// mgr.setVerbose(true);
	StateBlock blk1 (
	"0000000000000000000000000000000000000000000000000000000000000000",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000001",
	"000000000000000000000000000000000000000000000000000000000000000F");

	StateBlock blk2 (
	"0000000000000000000000000000000000000000000000000000000000000001",
	{}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000002",
	"000000000000000000000000000000000000000000000000000000000000000F");
	
	StateBlock blk3 (
	"0000000000000000000000000000000000000000000000000000000000000000",
	{"0000000000000000000000000000000000000000000000000000000000000001",
	 "0000000000000000000000000000000000000000000000000000000000000002"
	}, 
	{},
	"77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"0000000000000000000000000000000000000000000000000000000000000003",
	"000000000000000000000000000000000000000000000000000000000000000F");

	mgr->addStateBlock(blk3, true);
	mgr->addStateBlock(blk2, true);
	mgr->addStateBlock(blk1, true);
	std::string pivot;
	std::vector<std::string> tips;
	std::vector<Dag::vertex_t> criticals;
	mgr->getLatestPivotAndTips(pivot, tips);
	
	taraxa::thisThreadSleepForMicroSeconds(500);
	mgr->stop();
	EXPECT_EQ(mgr->getNumVerticesInDag(),4);
	EXPECT_EQ(mgr->getNumEdgesInDag(),5);
	EXPECT_EQ(mgr->getBufferSize(), 0);

}

/**
 * Note: TODO, Disable for now
 * The first thread has more change to win the Dag lock,
 * probably need to add some variation
 */
 
 /*
TEST(DagManager, receive_block_out_of_order_multi_thread){
	auto mgr = std::make_shared<DagManager> (2);

	//mgr->setVerbose(true);
	mgr->start();

	StateBlock blk1 (
	("0000000000000000000000000000000000000000000000000000000000000000"),
	{}, 
	{},
	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("0000000000000000000000000000000000000000000000000000000000000001"));

	StateBlock blk2 (
	("0000000000000000000000000000000000000000000000000000000000000001"),
	{}, 
	{},
	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("0000000000000000000000000000000000000000000000000000000000000002"));
	
	StateBlock blk3 (
	("0000000000000000000000000000000000000000000000000000000000000000"),
	{"0000000000000000000000000000000000000000000000000000000000000001",
	 "0000000000000000000000000000000000000000000000000000000000000002"
	}, 
	{},
	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("0000000000000000000000000000000000000000000000000000000000000003"));

	StateBlock blk4 (
	("0000000000000000000000000000000000000000000000000000000000000002"),
	{"0000000000000000000000000000000000000000000000000000000000000003",
	}, 
	{},
	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("0000000000000000000000000000000000000000000000000000000000000004"));
	
	StateBlock blk5 (
	("0000000000000000000000000000000000000000000000000000000000000004"),
	{"0000000000000000000000000000000000000000000000000000000000000003",
	}, 
	{},
	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("0000000000000000000000000000000000000000000000000000000000000005"));
	
	StateBlock blk6 (
	("0000000000000000000000000000000000000000000000000000000000000005"),
	{}, 
	{},
	("77777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777"),
	("0000000000000000000000000000000000000000000000000000000000000006"));
	
	mgr->addStateBlock(blk6, true);
	mgr->addStateBlock(blk5, true);
	mgr->addStateBlock(blk4, true);
	mgr->addStateBlock(blk3, true);
	mgr->addStateBlock(blk2, true);
	mgr->addStateBlock(blk1, true);
	

	thisThreadSleepForMicroSeconds(500);
	mgr->stop();
	EXPECT_EQ(mgr->getNumVerticesInDag(),7);
	EXPECT_EQ(mgr->getNumEdgesInDag(),10);
	EXPECT_EQ(mgr->getBufferSize(), 0);

}
*/

}  //namespace taraxa

int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}