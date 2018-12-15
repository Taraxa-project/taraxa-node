/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 13:23:51 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-14 18:29:38
 */
 
#include <iostream>
#include <string>
#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/depth_first_search.hpp>
#include "types.hpp"

namespace taraxa{

class MyVisitor : public boost::default_dfs_visitor 
{ 
public: 
	template < typename Vertex, typename Graph >
	void discover_vertex(Vertex u, const Graph & g) const;
	template < typename Vertex, typename Graph >
	void finish_vertex(Vertex u, const Graph & g) const;
	template < typename Edge, typename Graph >
	void examine_edge(Edge e, const Graph& g) const; 
};

class Dag {
public:
	// properties
	typedef boost::property<boost::vertex_index_t, blk_hash_t, 
		boost::property<boost::vertex_out_degree_t, uint32_t> > vertex_property_t;
	typedef boost::property<boost::edge_index_t, uint64_t> edge_property_t;
	
	// graph def
	typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, 
		vertex_property_t, edge_property_t> graph_t;
	typedef boost::graph_traits<graph_t>::vertex_descriptor vertex_t;
	typedef boost::graph_traits<graph_t>::edge_descriptor edge_t;
	typedef boost::graph_traits<graph_t>::vertex_iterator vertex_iter_t;
	typedef boost::graph_traits<graph_t>::edge_iterator edge_iter_t;

	// property_map
	typedef boost::property_map<graph_t, boost::vertex_index_t>::type vertex_index_map_t;
	typedef boost::property_map<graph_t, boost::vertex_out_degree_t>::type vertex_out_degree_map_t;
	typedef boost::property_map<graph_t, boost::edge_index_t>::type edge_index_map_t;

	vertex_t addVertex(blk_hash_t v);
	edge_t   addEdge(vertex_t from, vertex_t to);
	void dfs() const;
	void printAllVertexes() const;

private:
	graph_t graph_;


};
}
