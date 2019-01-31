/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 13:23:51 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-31 00:00:55
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
#include "util.hpp"
#include "state_block.hpp"
#include <atomic>
#include <mutex>

#include <chrono>

namespace taraxa{

/** 
 * Thread safe. Labelled graph.
 */

class Dag {
public:
	// properties
	using vertex_hash = std::string;
	using vertex_property_t = boost::property<boost::vertex_name_t, std::string, 
		boost::property<boost::vertex_index1_t, time_stamp_t>>;
	using edge_property_t = boost::property<boost::edge_index_t, uint64_t>;
	
	// graph def
	using adj_list_t = boost::adjacency_list<boost::setS, boost::vecS, boost::directedS, 
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

	using vertex_time_stamp_map_const_t = boost::property_map<graph_t, boost::vertex_index1_t>::const_type;
	using vertex_time_stamp_map_t = boost::property_map<graph_t, boost::vertex_index1_t>::type;

	using edge_index_map_const_t = boost::property_map<graph_t, boost::edge_index_t>::const_type;
	using edge_index_map_t = boost::property_map<graph_t, boost::edge_index_t>::type;
	
	using ulock = std::unique_lock<std::mutex>;  
	
	static const std::string GENESIS;

	Dag ();
	~Dag ();
	void setVerbose(bool verbose);
	void setDebug(bool debug);
	static vertex_hash getGenesis();
	uint64_t getNumVertices() const;  
	uint64_t getNumEdges() const;  
	bool hasVertex(vertex_hash const & v) const;
	// VEE: new vertex, pivot, tips
	// Note: *** The function does not check vertex existency
	bool addVEEs(vertex_hash const & new_vertex, vertex_hash const & pivot, 
		std::vector<vertex_hash> const & tips);
	void collectTips(std::vector<vertex_hash> & tips) const;
	void collectPivot(vertex_hash & pivot) const;
	
	void drawGraph(vertex_hash filename) const;
	
	void getChildrenBeforeTimeStamp(vertex_hash const & vertex, time_stamp_t stamp, std::vector<vertex_hash> & children) const;
	// Computational heavy
	void getTipsBeforeTimeStamp(vertex_hash const & veretx, time_stamp_t stamp, std::vector<vertex_hash> & tips) const;
	time_stamp_t getVertexTimeStamp(vertex_hash const & vertex) const;
	// Create critical section, the function should be used to add StateBlock
	
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
	// Note: private functions does not lock

	// vertex API
	vertex_t addVertex(std::string const & v);
	
	// edge API
	edge_t   addEdge(std::string const & v1, std::string const & v2);
	edge_t   addEdge(vertex_t v1, vertex_t v2);

	// traverser API
	void collectLeafVertices(std::vector<vertex_t> &leaves) const;
	void collectCriticalPath(std::vector<vertex_t> &criticals) const;


	bool debug_;
	bool verbose_;
	graph_t graph_;
	vertex_t genesis_; // root node
	mutable std::mutex mutex_;
	mutable std::mutex debug_mutex_;
};

class SbBuffer;
class TipBlockExplorer;
/**
 * Important : The input StateBlocks to DagManger should be de-duplicated!
 * i.e., no same StateBlocks are put to the Dag.
 */

class DagManager : public std::enable_shared_from_this<DagManager> {
public:
	using ulock = std::unique_lock<std::mutex>;  

	DagManager(unsigned num_threads);
	~DagManager();
	void start();
	void stop();
	std::shared_ptr<DagManager> getShared();

	bool addStateBlock(StateBlock const &blk, bool insert); // insert to buffer if fail
	void consume(unsigned threadId);
	void getLatestPivotAndTips(std::string & pivot, std::vector<std::string> & tips) const;
	
	// debug functions
	std::vector<std::string> getChildrenBeforeTimeStamp(std::string const & veretx, time_stamp_t stamp) const;
	std::vector<std::string> getTipsBeforeTimeStamp(std::string const & veretx, time_stamp_t stamp) const;
	time_stamp_t getStateBlockTimeStamp(std::string const & vertex);
	uint64_t getNumVerticesInDag() const ;
	uint64_t getNumEdgesInDag() const ;

	void setDebug(bool debug);
	void setVerbose(bool verbose);
	size_t getBufferSize() const;
	void drawGraph(std::string const & str) const;
	
private:
	void addToSbBuffer(StateBlock const & blk);
	void addToDag(std::string const &hash, 
		std::string const & pivot, std::vector<std::string> const & tips);
	unsigned getBlockInsertingIndex(); // add to block to different array
	void countFreshBlocks();
	bool debug_;
	bool verbose_;
	bool dag_updated_;
	bool on_;
	unsigned num_threads_;
	mutable std::mutex mutex_;
	std::atomic<unsigned> inserting_index_counter_;
	std::shared_ptr<Dag> dag_;
	std::shared_ptr<TipBlockExplorer> tips_explorer_;
	// SbBuffer 
	std::shared_ptr<std::vector<SbBuffer>> sb_buffer_array_;
	std::vector<boost::thread> sb_buffer_processing_threads_;
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

/**
 * Thread-safe . Only one thread can access ready. 
 * Will block if data is not ready
 */

class TipBlockExplorer{
public:
	using ulock = std::unique_lock<std::mutex>;
	TipBlockExplorer(unsigned rate);
	~TipBlockExplorer();
	void blockAdded();	
	// will block if not ready.
	void waitForReady();
private: 
	bool ready_;
	bool on_;
	unsigned rate_limit_;
    unsigned counter_;
	std::mutex mutex_;
	std::condition_variable condition_;
};


}
