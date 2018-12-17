/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 10:59:17 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-17 15:39:18
 */
 
#include "dag.hpp"
#include <tuple>
#include <fstream>

namespace taraxa {

Dag::Dag(){
	genesis_ = addVertex("0000");
}
Dag::~Dag(){}
Dag::graph_t Dag::getGraph() { return graph_;}

Dag::vertex_t Dag::getGenesis() {return genesis_;}

Dag::vertex_t Dag::addVertex(blk_hash_t hash){
	vertex_t ret=boost::add_vertex(hash, graph_);
	return ret;
}
Dag::edge_t Dag::addEdge(Dag::vertex_t v1, Dag::vertex_t v2){
	auto ret= boost::add_edge(v1, v2, graph_);
	assert(ret.second);
	return ret.first;
}

void Dag::collectLeafVertexes(std::vector<vertex_t> &leaves) const{
	leaves.clear();
	vertex_iter_t s, e;
	// iterator all vertex
	for (std::tie(s,e) = boost::vertices(graph_); s!=e; ++s){
		// if out-degree zero, leaf node
		if (boost::out_degree(*s, graph_)==0){
			leaves.push_back(*s);
		}
	}

}
void Dag::collectCriticalPath(std::vector<vertex_t> &critical_path) const{
	critical_path.clear();
	// starting from genesis node
	vertex_t current = genesis_;
	while (boost::out_degree(current, graph_)){
		vertex_adj_iter_t critical;
		std::tie(critical, std::ignore) = boost::adjacent_vertices(current, graph_);
		vertex_adj_iter_t s, e;
		for (std::tie(s,e) = boost::adjacent_vertices(current, graph_); s!=e; s++){
			size_t current_vertex_deg = boost::out_degree(*s, graph_);
			size_t critical_vertex_deg = boost::out_degree(*critical, graph_);
			if (current_vertex_deg >critical_vertex_deg){
				critical = s;
			}
			// tie breaker, choose the one has smaller hash
			else if (current_vertex_deg == critical_vertex_deg){
				// this is a const member function, need const type
				vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
				if (name_map[*s]<name_map[*critical]){
					critical = s;
				}
			}
		}
		critical_path.push_back(*critical);
		current = *critical;
	}
}

void Dag::drawGraph(std::string filename) const{
	std::ofstream outfile(filename.c_str());
	auto name_map = boost::get(boost::vertex_name, graph_);
	boost::write_graphviz(outfile, graph_, make_label_writer(name_map)); 
	std::cout<<"Dot file "<<filename<< " generated!"<<std::endl;
	std::cout<<"Use \"dot -Tps <dot file> -o <ps file>\" to generate ps file"<<std::endl;
}


}

int main(int argc, char *argv[]){
	
	taraxa::Dag graph;
	auto v0= graph.addVertex("10");
	auto v1= graph.addVertex("1");
	auto v2= graph.addVertex("2888");
	auto v3= graph.addVertex("35");
	auto genesis = graph.getGenesis();
	graph.addEdge(genesis, v0);
	graph.addEdge(v0, v1);
	graph.addEdge(v0, v2);
	graph.addEdge(v2, v3);
	std::vector<taraxa::Dag::vertex_t> leaves, critical;
	graph.collectLeafVertexes(leaves);
	graph.collectCriticalPath(critical);
	auto v = boost::get(boost::vertex_name, graph.getGraph());

	
	for (auto e:leaves)
		std::cout<<"Leaf "<<e<<" "<<v[e]<<std::endl; 
	for (auto e:critical)
		std::cout<<"Critical "<<e<<" "<<v[e]<<std::endl; 

	graph.drawGraph("draw.dot");
	return 0;
}