#include <gtest/gtest.h>
#include "dag.hpp"
#include "types.hpp"
namespace taraxa{
TEST(Dag, build_graph){
	taraxa::Dag graph;
	// a genesis vertex
	ASSERT_EQ(1, graph.getNumVertices()); 
	auto v0= graph.addVertex(blk_hash_t("1000000000000000000000000000000000000000000000000000000000000000"));
	auto v1= graph.addVertex(blk_hash_t("1111111111111111111111111111111111111111111111111111111111111111"));
	auto v2= graph.addVertex(blk_hash_t("2222222222222222222222222222222222222222222222222222222222222222"));
	auto v3= graph.addVertex(blk_hash_t("3333333333333333333333333333333333333333333333333333333333333333"));
	ASSERT_EQ(5, graph.getNumVertices());
	auto genesis = graph.getGenesis();
	graph.addEdge(genesis, v0);
	graph.addEdge(v0, v1);
	graph.addEdge(v0, v2);
	graph.addEdge(v2, v3);
	ASSERT_EQ(4, graph.getNumEdges());
}

TEST(Dag, traversal){
	taraxa::Dag graph;
	auto v0 = graph.addVertex(blk_hash_t("0000000000000000000000000000000000000000000000000000000000000000"));
	auto v1 = graph.addVertex(blk_hash_t("1111111111111111111111111111111111111111111111111111111111111111"));
	auto v2 = graph.addVertex(blk_hash_t("2222222222222222222222222222222222222222222222222222222222222222"));
	auto v3 = graph.addVertex(blk_hash_t("3333333333333333333333333333333333333333333333333333333333333333"));
	// unconnected vertex
	auto v4 = graph.addVertex(blk_hash_t("9999999999999999999999999999999999999999999999999999999999999999")); 
	ASSERT_EQ(6, graph.getNumVertices());
	auto genesis = graph.getGenesis();
	graph.addEdge(genesis, v0);
	graph.addEdge(v0, v1);
	graph.addEdge(v0, v2);
	graph.addEdge(v2, v3);
	ASSERT_EQ(4, graph.getNumEdges());

	std::vector<taraxa::Dag::vertex_t> leaves, critical;
	graph.collectLeafVertices(leaves);
	ASSERT_EQ(3, leaves.size());
	graph.collectCriticalPath(critical);
	ASSERT_EQ(3, critical.size());
}

}  //namespace taraxa

int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}