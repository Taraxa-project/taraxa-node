/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 10:59:17 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-17 16:13:22
 */
 
#include <tuple>
#include <fstream>
#include <vector>

#include "dag.hpp"

namespace taraxa {

Dag::Dag(){
	genesis_ = addVertex("0000000000000000000000000000000000000000000000000000000000000000");
}
Dag::~Dag(){}
Dag::graph_t & Dag::getGraph() { return graph_;}

Dag::vertex_t Dag::getGenesis() {return genesis_;}
uint64_t Dag::getNumVertices() const {
	ulock lock(mutex_);
	return boost::num_vertices(graph_);
}
uint64_t Dag::getNumEdges() const {
	ulock lock(mutex_);
	return boost::num_edges(graph_);
}
Dag::vertex_t Dag::addVertex(std::string const & hash){
	ulock lock(mutex_);
	vertex_t ret= add_vertex(hash, graph_);
	vertex_name_map_t name_map = boost::get(boost::vertex_name, graph_);
	name_map[ret]=hash;
	return ret;
}
Dag::edge_t Dag::addEdge(Dag::vertex_t v1, Dag::vertex_t v2){
	ulock lock(mutex_);
	auto ret = boost::add_edge(v1, v2, graph_);
	assert(ret.second);
	return ret.first;
}

Dag::edge_t Dag::addEdge(std::string const & from, std::string const & to){
	
	assert(hasVertex(from));
	assert(hasVertex(to));
	
	// lock should be behind assert, o.w. deadlock
	ulock lock(mutex_);

	edge_t edge;
	bool res;
 	std::tie(edge, res) = add_edge_by_label(from, to, graph_);
	assert(res);
	return edge;
}

bool Dag::hasVertex(std::string const & hash){
	ulock lock(mutex_);
	return graph_.vertex(hash) != graph_.null_vertex();
}

void Dag::collectLeafVertices(std::vector<vertex_t> &leaves) const{
	ulock lock(mutex_);
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
	ulock lock(mutex_);
	critical_path.clear();
	// starting from genesis node
	vertex_t current = genesis_;
	while (boost::out_degree(current, graph_)){
		vertex_adj_iter_t critical;
		std::tie(critical, std::ignore) = adjacenct_vertices(current, graph_);
		vertex_adj_iter_t s, e;
		for (std::tie(s,e) = adjacenct_vertices(current, graph_); s!=e; s++){
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
 	ulock lock(mutex_);
	std::ofstream outfile(filename.c_str());
	auto name_map = boost::get(boost::vertex_name, graph_);
	boost::write_graphviz(outfile, graph_, make_label_writer(name_map)); 
	std::cout<<"Dot file "<<filename<< " generated!"<<std::endl;
	std::cout<<"Use \"dot -Tpdf <dot file> -o <pdf file>\" to generate ps file"<<std::endl;
}

DagManager::DagManager(unsigned num_threads): 
	debug_(false),
	verbose_(false),
	dag_updated_(false),
	on_(true),
	num_threads_(num_threads),
	counter_(0),
	dag_(std::make_shared<Dag>()),
	sb_buffer_array_(std::make_shared<std::vector<SbBuffer>> (num_threads)){
	for (auto i = 0; i < num_threads; ++i){
		processing_threads_.push_back(boost::thread([this, i](){
			consume(i);
		}));
	}
}
DagManager::~DagManager(){
	for (auto & t: processing_threads_){
		t.join();
	}
}
void DagManager::setDebug(bool debug) { debug_ = debug;}
void DagManager::setVerbose(bool verbose) {verbose_ = verbose;}
uint64_t DagManager::getNumVerticesInDag() {return dag_->getNumVertices();}
uint64_t DagManager::getNumEdgesInDag() { return dag_->getNumEdges();}
size_t DagManager::getBufferSize() {
	auto sz=0;
	for (auto const & arr: (*sb_buffer_array_)){
		sz += arr.size();
	}
		return sz;
}
unsigned DagManager::getBlockInsertingIndex(){
	auto which_buffer = counter_.fetch_add(1);
	assert(which_buffer<num_threads_ && which_buffer >= 0);

	if (counter_>=num_threads_){
		counter_.store(0);
	}
	if (verbose_){
		std::cout<<"inserting row = "<<which_buffer<<"\n";
	}
	return which_buffer;
}
void DagManager::drawGraph(std::string const & str){
	dag_->drawGraph(str);
}

void DagManager::stop() { 
	on_ = false;
	for (auto & arr: (*sb_buffer_array_)){
		arr.stop();
	}
	//sb_buffer_array_->stop();
}
bool DagManager::addStateBlock(StateBlock const &blk, bool insert){
	std::string hash = blk.getHash().toString();
	if (dag_->hasVertex(hash)){
		if (verbose_){
			std::cout<<"Block is in DAG already! "<<hash<<std::endl;
		}
		return false;
	}

	std::string pivot = blk.getPivot().toString();
	if (!dag_->hasVertex(pivot)){
		if (verbose_){
			std::cout<<"Block "<<hash<<" pivot "<<pivot<<" unavailable, insert = "<<insert<<std::endl;
		}
		if (insert){
			unsigned which_buffer = getBlockInsertingIndex();
			(*sb_buffer_array_)[which_buffer].insert(blk);
		}
		return false;
	}
	
	std::vector<std::string> tips;
	for (auto const & t: blk.getTips()){
		std::string tip = t.toString();
		if (!dag_->hasVertex(tip)){
			if (verbose_){
				std::cout<<"Block "<<hash<<" tip "<<tip<<" unavailable, insert = "<<insert<<std::endl;
			}
			if (insert){
				unsigned which_buffer = getBlockInsertingIndex();
				(*sb_buffer_array_)[which_buffer].insert(blk);
			}
			return false;
		}
		tips.push_back(tip);
	} 

	// construct a new node and the edges 
	dag_->addVertex(hash);

	assert(blk.getTips().size()==tips.size());
	dag_->addEdge(hash, pivot);

	for (auto const &t: tips){
		dag_->addEdge(hash, t);
	}
	
	return true;
}

void DagManager::consume(unsigned idx){

	while(on_){
		auto iter = (*sb_buffer_array_)[idx].getBuffer();
		auto & blk = iter->first;
		if (!on_) break;
		if (addStateBlock(blk, false)){
			// std::cout<<"consume success ..."<<blk.getHash().toString()<<std::endl;
			(*sb_buffer_array_)[idx].delBuffer(iter);
		}
	}
}


SbBuffer::SbBuffer(): stopped_(false), updated_(false), iter_(blocks_.end()){}

void SbBuffer::stop() { 

	stopped_ = true;
	condition_.notify_all();
}

bool SbBuffer::isStopped() const {return stopped_;}

void SbBuffer::insert(StateBlock const & blk){
	ulock lock(mutex_);
	time_point_t t = std::chrono::steady_clock::now ();
	blocks_.emplace_front(std::make_pair(blk, t));
	updated_=true;
	condition_.notify_one();
}

SbBuffer::buffIter SbBuffer::getBuffer(){
	ulock lock(mutex_);
	while (!stopped_ && (blocks_.empty() || (iter_ == blocks_.end() && !updated_) )){
		condition_.wait(lock);
	}
	if (iter_ == blocks_.end()){
		iter_ = blocks_.begin();
	}
	auto cur=iter_;
	iter_++;
	return cur;

}
void SbBuffer::delBuffer(SbBuffer::buffIter iter){
	ulock lock(mutex_);
	blocks_.erase(iter);
}

size_t SbBuffer::size() const { return blocks_.size();}

}