/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 13:23:51 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-18 14:18:42
 */
 
#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <boost/graph/properties.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/graphviz.hpp>
#include "types.hpp"

namespace taraxa{

class Dag {
public:
	// properties
	// typedef boost::property<boost::vertex_name_t, blk_hash_t, 
	// 	boost::property<boost::vertex_out_degree_t, uint32_t> > vertex_property_t;
	typedef boost::property<boost::vertex_name_t, blk_hash_t> vertex_property_t;
	typedef boost::property<boost::edge_index_t, uint64_t> edge_property_t;
	
	// graph def
	typedef boost::adjacency_list<boost::listS, boost::vecS, boost::directedS, 
		vertex_property_t, edge_property_t> graph_t;
	typedef boost::graph_traits<graph_t>::vertex_descriptor vertex_t;
	typedef boost::graph_traits<graph_t>::edge_descriptor edge_t;
	typedef boost::graph_traits<graph_t>::vertex_iterator vertex_iter_t;
	typedef boost::graph_traits<graph_t>::edge_iterator edge_iter_t;
	typedef boost::graph_traits<graph_t>::adjacency_iterator vertex_adj_iter_t;


	// property_map
	typedef boost::property_map<graph_t, boost::vertex_name_t>::const_type vertex_name_map_const_t;
	typedef boost::property_map<graph_t, boost::vertex_name_t>::type vertex_name_map_t;
	typedef boost::property_map<graph_t, boost::edge_index_t>::const_type edge_index_map_const_t;
	typedef boost::property_map<graph_t, boost::edge_index_t>::type edge_index_map_t;
	Dag ();
	~Dag ();
	graph_t getGraph();
	vertex_t getGenesis(); // The root node
	uint64_t getNumVertices() const;  
	uint64_t getNumEdges() const;  
	vertex_t addVertex(blk_hash_t v);
	edge_t   addEdge(vertex_t from, vertex_t to);
	void dfs() const;
	void collectLeafVertices(std::vector<vertex_t> &leaves) const;
	void collectCriticalPath(std::vector<vertex_t> &leaves) const;
	void drawGraph(std::string filename) const;

	// for graphviz
	template <class Property>
  	class label_writer {
  	public:
    	label_writer(Property property) : property(property) {}
    	template <class VertexOrEdge>
    	void operator()(std::ostream& out, const VertexOrEdge& v) const {
    		out << "[label=\"" << property[v] << "\"]";
    	}
	private:
		Property property;
  };
private:
	graph_t graph_;
	vertex_t genesis_; // root node

};
}
