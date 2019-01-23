/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 13:23:51 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-12-18 14:18:42
 */
 
#include <iostream>
#include <string>
#include <iterator>
#include <list>
#include <boost/thread.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/function.hpp>

#include <condition_variable>
#include "types.hpp"
#include "state_block.hpp"
#include <atomic>

namespace taraxa{

class Dag {
public:
	// properties
	// typedef boost::property<boost::vertex_name_t, blk_hash_t, 
	// 	boost::property<boost::vertex_out_degree_t, uint32_t> > vertex_property_t;
	using vertex_property_t = boost::property<boost::vertex_name_t, std::string>;
	using edge_property_t = boost::property<boost::edge_index_t, uint64_t>;
	
	// graph def
	using adj_list_t = boost::adjacency_list<boost::listS, boost::vecS, boost::directedS, 
		vertex_property_t, edge_property_t>;
	using graph_t = boost::labeled_graph<adj_list_t, std::string, boost::hash_mapS>;
	using vertex_t = boost::graph_traits<graph_t>::vertex_descriptor;
	using edge_t = boost::graph_traits<graph_t>::edge_descriptor;
	using vertex_iter_t = boost::graph_traits<graph_t>::vertex_iterator;
	using edge_iter_t = boost::graph_traits<graph_t>::edge_iterator;
	using vertex_adj_iter_t = boost::graph_traits<graph_t>::adjacency_iterator;


	// property_map
	using vertex_name_map_const_t = boost::property_map<graph_t, boost::vertex_name_t>::const_type;
	using vertex_name_map_t = boost::property_map<graph_t, boost::vertex_name_t>::type;
	using edge_index_map_const_t = boost::property_map<graph_t, boost::edge_index_t>::const_type;
	using edge_index_map_t = boost::property_map<graph_t, boost::edge_index_t>::type;
	
	using ulock = std::unique_lock<std::mutex>;  
	Dag ();
	~Dag ();
	graph_t & getGraph();
	vertex_t getGenesis(); // The root node
	uint64_t getNumVertices() const;  
	uint64_t getNumEdges() const;  
	vertex_t addVertex(std::string const & hash);
	edge_t   addEdge(vertex_t from, vertex_t to);
	edge_t   addEdge(std::string const & from, std::string const & to);
	bool hasVertex(std::string const & hash);
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
	mutable std::mutex mutex_;
};

class SbBuffer;

class DagManager {
public:
	DagManager(unsigned num_threads);
	~DagManager();
	bool addStateBlock(StateBlock const &blk, bool insert); // insert to buffer if fail
	void consume(unsigned threadId);
	uint64_t getNumVerticesInDag();
	uint64_t getNumEdgesInDag();
	void setDebug(bool debug);
	void setVerbose(bool verbose);
	void stop();
	size_t getBufferSize();
	void drawGraph(std::string const & str);
private:
	unsigned getBlockInsertingIndex(); // add to block to different array
	bool debug_;
	bool verbose_;
	bool dag_updated_;
	bool on_;
	unsigned num_threads_;
	std::atomic<unsigned> counter_;
	std::shared_ptr<Dag> dag_;
	// SbBuffer 
	std::shared_ptr<std::vector<SbBuffer>> sb_buffer_array_;
	std::vector<boost::thread> processing_threads_;
};

// Thread safe buffer for StateBlock

class SbBuffer{ 
public: 
	using stampedBlock = std::pair<StateBlock, time_point_t>; 
	using buffIter = std::list<stampedBlock>::iterator;
	using ulock = std::unique_lock<std::mutex>;
	SbBuffer();
	void insert(StateBlock const &blk);
	buffIter getBuffer();
	void delBuffer(buffIter);
	void stop();
	bool isStopped() const ;
	size_t size() const;
private:
	bool stopped_;
	bool updated_;
	std::list<stampedBlock> blocks_;
	std::condition_variable condition_;
	std::mutex mutex_;
	buffIter iter_;
};





}
