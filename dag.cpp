/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 10:59:17 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-14 18:28:54
 */
 
#include "dag.hpp"
namespace taraxa {
		
template < typename Vertex, typename Graph >
void MyVisitor::discover_vertex(Vertex u, const Graph & g) const { 
	std::cout << "At " << u << std::endl; 
}
template < typename Vertex, typename Graph >
void MyVisitor::finish_vertex(Vertex u, const Graph & g) const { 
	std::cout << "Exit " << u << std::endl; 
}
template < typename Edge, typename Graph >
void MyVisitor::examine_edge(Edge e, const Graph& g) const { 
	std::cout << "Examining edges " << e << std::endl;
}

// class MyVisitor : public boost::default_dfs_visitor
// {
// public:
//   void discover_vertex(Dag::vertex_t v, const Dag::graph_t & g) const
//   {
//     //std::cout << v << std::endl;
//     return;
//   }
// };

Dag::vertex_t Dag::addVertex(blk_hash_t hash){
	vertex_t ret=boost::add_vertex(hash, graph_);
	return ret;
}
Dag::edge_t Dag::addEdge(Dag::vertex_t v1, Dag::vertex_t v2){
	auto ret= boost::add_edge(v1, v2, graph_);
	assert(ret.second);
	return ret.first;
}
void Dag::printAllVertexes() const{
	auto index_map = boost::get(boost::vertex_index, graph_);
	for (auto vp= boost::vertices(graph_); vp.first!=vp.second; ++vp.first){
		std::cout<<"Vertex("<<index_map[*vp.first]<<")\n";
	}
}

void Dag::dfs() const{
	MyVisitor vis;
	auto index_map = boost::get(boost::vertex_index, graph_);
	boost::depth_first_search(graph_, boost::visitor(vis));
}


}

int main(int argc, char *argv[]){
	taraxa::Dag graph;

	auto v0= graph.addVertex("0");
	auto v1= graph.addVertex("1");
	graph.addEdge(v0, v1);
	graph.printAllVertexes();
	graph.dfs();
	return 0;
}