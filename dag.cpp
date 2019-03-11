/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-12-14 10:59:17 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-25 22:56:30
 */
 
#include <tuple>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <queue>

#include "dag.hpp"

namespace taraxa {

std::string const Dag::GENESIS = "0000000000000000000000000000000000000000000000000000000000000000";

Dag::Dag(): debug_(false), verbose_(false){
	vertex_hash pivot="";
	std::vector<vertex_hash> tips;
	genesis_ = addVEEs(Dag::GENESIS, pivot, tips);
}
Dag::~Dag(){}
void Dag::setVerbose(bool verbose){ verbose_ = verbose;}
void Dag::setDebug(bool debug){ debug_ = debug;}

uint64_t Dag::getNumVertices() const {
	ulock lock(mutex_);
	return boost::num_vertices(graph_);
}
uint64_t Dag::getNumEdges() const {
	ulock lock(mutex_);
	return boost::num_edges(graph_);
}

bool Dag::hasVertex(vertex_hash const & v) const{
	ulock lock(mutex_);
	return graph_.vertex(v) != graph_.null_vertex();
}

void Dag::collectLeaves(std::vector<vertex_hash> & tips) const{
	ulock lock(mutex_);
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
	std::vector<vertex_t> leaves;
	collectLeafVertices(leaves);
	for (auto const & leaf: leaves){
		tips.push_back(name_map[leaf]);
	}
}

bool Dag::addVEEs(vertex_hash const & new_vertex, vertex_hash const & pivot, 
	std::vector<vertex_hash> const & tips){
	
	ulock lock(mutex_);
	assert(!new_vertex.empty());
	
	// add vertex
	auto now (std::chrono::system_clock::now());
	vertex_t ret= add_vertex(new_vertex, graph_);
	vertex_name_map_t name_map = boost::get(boost::vertex_name, graph_);
	name_map[ret]=new_vertex;
	vertex_time_stamp_map_t stamp_map = boost::get(boost::vertex_index1, graph_);
	stamp_map[ret] = getTimePoint2Long(now);
	// std::cout<<"Created vertex "<< new_vertex<< " at "<< getTimePoint2Long(now) <<std::endl;
	edge_t edge;
	bool res;

	// Note: add edges, 
	// *** important
	// Add a new block, edges are pointing from pivot to new_veretx
	if (!pivot.empty()){
		std::tie(edge, res) = add_edge_by_label(pivot, new_vertex, graph_);
		if (!res){
			std::cout<<"Warning! creating pivot edge \n"<< pivot << "\n-->\n"
			<<new_vertex<<" \nunsuccessful!"<<std::endl;
		}
	}
	bool res2;
	for (auto const & e: tips){
 		std::tie(edge, res2) = add_edge_by_label(e, new_vertex, graph_);
		if (!res2){
			std::cout<<"Warning! creating tip edge \n"<< e << "\n-->\n"
			<<new_vertex<<" \nunsuccessful!"<<std::endl;
		}
	}
	return res & res2;
}

void Dag::drawGraph(std::string filename) const{
 	ulock lock(mutex_);
	std::ofstream outfile(filename.c_str());
	auto name_map = boost::get(boost::vertex_name, graph_);
	boost::write_graphviz(outfile, graph_, make_label_writer(name_map)); 
	std::cout<<"Dot file "<<filename<< " generated!"<<std::endl;
	std::cout<<"Use \"dot -Tpdf <dot file> -o <pdf file>\" to generate pdf file"<<std::endl;
}

Dag::vertex_t Dag::addVertex(vertex_hash const & hash){
	vertex_t ret= add_vertex(hash, graph_);
	vertex_name_map_t name_map = boost::get(boost::vertex_name, graph_);
	name_map[ret]=hash;
	return ret;
}
Dag::edge_t Dag::addEdge(Dag::vertex_t v1, Dag::vertex_t v2){
	auto ret = add_edge(v1, v2, graph_);
	assert(ret.second);
	return ret.first;
}

Dag::edge_t Dag::addEdge(vertex_hash const & v1, vertex_hash const & v2){
	
	assert(graph_.vertex(v1) != graph_.null_vertex());
	assert(graph_.vertex(v2) != graph_.null_vertex());	
	// lock should be behind assert
	edge_t edge;
	bool res;
 	std::tie(edge, res) = add_edge_by_label(v1, v2, graph_);
	assert(res);
	return edge;
}

void Dag::collectLeafVertices(std::vector<vertex_t> &leaves) const{
	leaves.clear();
	vertex_iter_t s, e;
	// iterator all vertex
	for (std::tie(s,e) = boost::vertices(graph_); s!=e; ++s){
		// if out-degree zero, leaf node
		if (boost::out_degree(*s, graph_)==0){
			leaves.push_back(*s);
		}
	}
	assert(leaves.size());
}

void Dag::getChildrenBeforeTimeStamp(vertex_hash const & vertex, time_stamp_t stamp, std::vector<vertex_hash> &children) const{
	ulock lock(mutex_);
	vertex_t current = graph_.vertex(vertex);
	if (current == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex "<<vertex<<"\n";
		return;
	}
	children.clear();

	vertex_time_stamp_map_const_t time_map = boost::get(boost::vertex_index1, graph_);
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
	vertex_adj_iter_t s, e;
	for (std::tie(s, e) = adjacenct_vertices(current, graph_); s!=e ; s++){
		if (time_map[*s] < stamp){
			children.push_back(name_map[*s]);
		}
	}
}

void Dag::getSubtreeBeforeTimeStamp(vertex_hash const & vertex, time_stamp_t stamp, std::vector<vertex_hash> &subtree) const{
	ulock lock(mutex_);
	vertex_t current = graph_.vertex(vertex);
	if (current == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex "<<vertex<<"\n";
		return;
	}
	subtree.clear();
	std::deque<vertex_t> q;
	std::set<vertex_t> visited;
	visited.insert(current);
	q.push_back(current);
	vertex_time_stamp_map_const_t time_map = boost::get(boost::vertex_index1, graph_);
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
	vertex_adj_iter_t s, e;

	while (!q.empty()){
		current = q.front();
		q.pop_front();
		for (std::tie(s, e) = adjacenct_vertices(current, graph_); s!=e ; s++){
			if (visited.count(*s)) {
				continue;
			}
			if (time_map[*s] >= stamp){
				continue;
			}
			visited.insert(*s);
			q.push_back(*s);
			subtree.push_back(name_map[*s]);
		}
	}
}

void Dag::getLeavesBeforeTimeStamp(vertex_hash const & vertex, time_stamp_t stamp, std::vector<vertex_hash> & tips) const{
	ulock lock(mutex_);
	vertex_t current = graph_.vertex(vertex);
	if (current == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex "<<vertex<<"\n";
		return;
	}
	tips.clear();
	vertex_time_stamp_map_const_t time_map = boost::get(boost::vertex_index1, graph_);
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
	vertex_adj_iter_t s, e;
	std::unordered_set<vertex_t> visited; 
	std::queue<vertex_t> qu;
	visited.insert(current);
	qu.emplace(current);
	while(!qu.empty()){
		vertex_t c = qu.front();
		qu.pop();
		size_t valid_children = 0;
		for (std::tie(s, e) = adjacenct_vertices(c, graph_); s != e; s++){
			if (time_map[*s]<stamp){
				valid_children++;
			}
			
			if (visited.count(*s)){
				continue;
			}

			visited.insert(*s);

			// old children, still need to explore children
			if (time_map[*s]<stamp){
				qu.push(*s);
			}
		}
		// time sense leaf
		if (valid_children == 0 && time_map[c]<stamp){
			tips.push_back(name_map[c]);
		}
	}
}


void Dag::getEpochVertices(vertex_hash const & from, vertex_hash const & to,std::vector<vertex_hash> & epochs) const{

	ulock lock(mutex_);
	
	vertex_t source = graph_.vertex(from);
	vertex_t target = graph_.vertex(to);

	if (source == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex (from)"<<from<<"\n";
		return;
	}
	if (target == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex (to) "<<to<<"\n";
		return;
	}
	epochs.clear();

	vertex_iter_t s, e;
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);

	// iterator all vertex
	for (std::tie(s,e) = boost::vertices(graph_); s!=e; ++s){
		if (*s==source || *s==target) continue;
		if (!reachable(*s, source) && reachable(*s, target)){
			epochs.push_back(name_map[*s]);
		}
	}	
}

time_stamp_t Dag::getVertexTimeStamp(vertex_hash const & vertex) const{
	ulock lock(mutex_);
	vertex_t current = graph_.vertex(vertex);
	if (current == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex "<<vertex<<"\n";
		return 0;
	}
	vertex_time_stamp_map_const_t time_map = boost::get(boost::vertex_index1, graph_);
	return time_map[current];
}

void Dag::setVertexTimeStamp(vertex_hash const & vertex, time_stamp_t stamp){
	ulock lock(mutex_);
	vertex_t current = graph_.vertex(vertex);
	if (current == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex "<<vertex<<"\n";
		return;
	}
	vertex_time_stamp_map_t time_map = boost::get(boost::vertex_index1, graph_);
	assert(stamp>=0);
	time_map[current] = stamp;
}

size_t Dag::outDegreeBeforeTimeStamp(vertex_t vertex, time_stamp_t stamp) const{

	size_t deg = 0;
	vertex_time_stamp_map_const_t time_map = boost::get(boost::vertex_index1, graph_);
	vertex_adj_iter_t s, e;
	for (std::tie(s, e) = adjacenct_vertices(vertex, graph_); s != e; s++){
		if (time_map[*s]<stamp){
			deg++;
		}
	}
	return deg;
}

// dfs
bool Dag::reachable(vertex_t const & from, vertex_t const & to) const{
	
	if (from == to) return true;
	vertex_t current = from;
	vertex_t target = to;
	std::stack<vertex_t> st;
	std::set<vertex_t> visited;
	st.push(current);
	visited.insert(current);

	while (!st.empty()){
		vertex_t t=st.top();
		st.pop();
		vertex_adj_iter_t s,e;
		for(std::tie(s,e) = adjacenct_vertices(t, graph_); s != e; ++s){
			if (visited.count(*s)) continue;
			if (*s == target) return true;
			visited.insert(*s);
			st.push(*s);
		}
	}
	return false;
}

// TODO: optimze 
void PivotTree::getHeavySubtreePath(vertex_hash const & vertex, std::vector<vertex_hash> &pivot_chain) const{
	ulock lock(mutex_);
	std::vector<vertex_t> results;
	vertex_t current = graph_.vertex(vertex);
	if (current == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex "<<vertex<<"\n";
		return;
	}
	pivot_chain.clear();
	heavySubtree(current, std::numeric_limits<uint64_t>::max(), results);
	std::reverse(results.begin(), results.end());
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);

	for (vertex_t const & r: results){
		pivot_chain.push_back(name_map[r]);
	}

}

void PivotTree::getHeavySubtreePathBeforeTimeStamp(vertex_hash const & vertex, time_stamp_t stamp, std::vector<vertex_hash> &pivot_chain) const{
	ulock lock(mutex_);
	std::vector<vertex_t> results;
	vertex_t current = graph_.vertex(vertex);
	if (current == graph_.null_vertex()){
		std::cout<<"Warning! cannot find vertex "<<vertex<<"\n";
		return;
	}
	pivot_chain.clear();
	heavySubtree(current, stamp, results);
	std::reverse(results.begin(), results.end());
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);

	for (vertex_t const & r: results){
		pivot_chain.push_back(name_map[r]);
	}

}
// TODO:: recursive, change to iterative ..
size_t PivotTree::heavySubtree(vertex_t const &vertex, time_stamp_t stamp, std::vector<vertex_t> &pivot_chain) const {
	
	if (outDegreeBeforeTimeStamp(vertex, stamp) == 0){
		pivot_chain={vertex};
		return 1;
	}
	vertex_adj_iter_t s, e;
	size_t total_weight = 0;
	size_t heavist_weight = 0;
	size_t heavist_child = vertex;
	vertex_name_map_const_t name_map = boost::get(boost::vertex_name, graph_);
	vertex_time_stamp_map_const_t time_map = boost::get(boost::vertex_index1, graph_);

	for (std::tie(s, e) = adjacenct_vertices(vertex, graph_); s!=e; s++){
		if (time_map[*s]>=stamp) continue;
		std::vector<vertex_t> sub_chain;
		size_t weight = heavySubtree(*s, stamp, sub_chain);
		if (weight > heavist_weight){
			heavist_weight = weight;
			heavist_child = *s;
			pivot_chain = sub_chain;
		}
		else if (weight == heavist_weight){ // compare hash
			if (name_map[*s]<name_map[heavist_child]){
				heavist_weight = weight;
				heavist_child = *s;
				pivot_chain = sub_chain;
			}
		}
		total_weight += weight;
	}
	total_weight++;
	pivot_chain.push_back(vertex);
	return total_weight;
}


DagManager::DagManager(unsigned num_threads) try : 
	debug_(false),
	verbose_(false),
	dag_updated_(false),
	on_(true),
	num_threads_(num_threads),
	inserting_index_counter_(0),
	total_dag_(std::make_shared<Dag>()),
	pivot_tree_(std::make_shared<PivotTree>()),
	tips_explorer_(std::make_shared<TipBlockExplorer>(3)),
	sb_buffer_array_(std::make_shared<std::vector<SbBuffer>> (num_threads)){	
} catch (std::exception & e){
	std::cerr<<e.what()<<std::endl;
}

DagManager::~DagManager(){
	for (auto & t: sb_buffer_processing_threads_){
		t.join();
	}
}

std::shared_ptr<DagManager> DagManager::getShared(){
	try{
		return shared_from_this();
	} catch( std::bad_weak_ptr & e){
		std::cerr<<"DagManager: "<<e.what()<<std::endl;
		return nullptr;
	}
}

void DagManager::setDebug(bool debug) { debug_ = debug;}
void DagManager::setVerbose(bool verbose) {verbose_ = verbose;}

std::pair<uint64_t, uint64_t> DagManager::getNumVerticesInDag() const {
	ulock lock(mutex_);
	return {total_dag_->getNumVertices(), pivot_tree_->getNumVertices()};
}

std::pair<uint64_t, uint64_t> DagManager::getNumEdgesInDag() const { 
	ulock lock(mutex_);
	return {total_dag_->getNumEdges(), pivot_tree_->getNumEdges()};
}
size_t DagManager::getBufferSize() const {
	ulock lock(mutex_);
	auto sz=0;
	for (auto const & arr: (*sb_buffer_array_)){
		sz += arr.size();
	}
	return sz;
}

unsigned DagManager::getBlockInsertingIndex(){
	auto which_buffer = inserting_index_counter_.fetch_add(1);
	assert(which_buffer<num_threads_ && which_buffer >= 0);

	if (inserting_index_counter_>=num_threads_){
		inserting_index_counter_.store(0);
	}
	if (verbose_){
		std::cout<<"inserting row = "<<which_buffer<<"\n";
	}
	return which_buffer;
}

void DagManager::drawTotalGraph(std::string const & str) const{
	ulock lock(mutex_);
	total_dag_->drawGraph(str);
}

void DagManager::drawPivotGraph(std::string const & str) const{
	ulock lock(mutex_);
	pivot_tree_->drawGraph(str);
}

void DagManager::start() {
	for (auto i = 0; i < num_threads_; ++i){
		sb_buffer_processing_threads_.push_back(boost::thread([this, i](){
			consume(i);
		}));
	}
}
void DagManager::stop() { 
	on_ = false;
	for (auto & arr: (*sb_buffer_array_)){
		arr.stop();
	}
}
bool DagManager::addStateBlock(StateBlock const &blk, bool insert){
	ulock lock(mutex_);
	std::string hash = blk.getHash().toString();
	if (total_dag_->hasVertex(hash)){
		if (verbose_){
			std::cout<<"Block is in DAG already! "<<hash<<std::endl;
		}
		return false;
	}

	std::string pivot = blk.getPivot().toString();
	if (!total_dag_->hasVertex(pivot)){
		if (verbose_){
			std::cout<<"Block "<<hash<<" pivot "<<pivot<<" unavailable, insert = "<<insert<<std::endl;
		}
		if (insert){
			addToSbBuffer(blk);
		}
		return false;
	}
	
	std::vector<std::string> tips;
	for (auto const & t: blk.getTips()){
		std::string tip = t.toString();
		if (!total_dag_->hasVertex(tip)){
			if (verbose_){
				std::cout<<"Block "<<hash<<" tip "<<tip<<" unavailable, insert = "<<insert<<std::endl;
			}
			if (insert){
				addToSbBuffer(blk);
			}
			return false;
		}
		tips.push_back(tip);
	} 

	addToDag(hash, pivot, tips);
	return true;
}

void DagManager::addToSbBuffer(StateBlock const & blk){
	unsigned which_buffer = getBlockInsertingIndex();
	(*sb_buffer_array_)[which_buffer].insert(blk);
}

void DagManager::addToDag(std::string const & hash, std::string const & pivot, 
	std::vector<std::string> const & tips){
	
	total_dag_->addVEEs(hash, pivot, tips);
	pivot_tree_->addVEEs(hash, pivot, {});
	// sync pivot tree's time stamp with total_dag_ timestamp
	auto stamp = total_dag_->getVertexTimeStamp(hash);
	pivot_tree_->setVertexTimeStamp(hash, stamp);
	// TODO: disconnect tip explorer for now. Reconnect later ...
	//tips_explorer_->blockAdded();
}


/**
 * TODO: current will do spining access, change it
 */
 
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
void DagManager::getLatestPivotAndTips(std::string & pivot, std::vector<std::string> & tips) const{
	// will block if not ready.
	tips_explorer_->waitForReady();
	// make sure the state of dag is the same when collection pivot and tips
	ulock lock(mutex_);
	std::vector<std::string> pivot_chain;
	pivot_tree_->getHeavySubtreePath(Dag::GENESIS, pivot_chain);
	pivot = pivot_chain.back();
	total_dag_->collectLeaves(tips);
}

std::vector<std::string> DagManager::getPivotChildrenBeforeTimeStamp(std::string const & vertex, time_stamp_t stamp) const{
	std::vector<std::string> ret;
	pivot_tree_->getChildrenBeforeTimeStamp(vertex, stamp, ret);
	return ret;
}

std::vector<std::string> DagManager::getPivotSubtreeBeforeTimeStamp(std::string const & vertex, time_stamp_t stamp) const{
	std::vector<std::string> ret;
	pivot_tree_->getSubtreeBeforeTimeStamp(vertex, stamp, ret);
	return ret;
}

std::vector<std::string> DagManager::getTotalLeavesBeforeTimeStamp(std::string const & vertex, time_stamp_t stamp) const{
	std::vector<std::string> ret;
	total_dag_->getLeavesBeforeTimeStamp(vertex, stamp, ret);
	return ret;
}

std::vector<std::string> DagManager::getTotalEpochsBetweenBlocks(std::string const & from, std::string const & to) const{
	std::vector<std::string> ret;
	total_dag_->getEpochVertices(from, to, ret);
	return ret;
}

std::vector<std::string> DagManager::getPivotChainBeforeTimeStamp(std::string const & vertex, time_stamp_t stamp) const {
	std::vector<std::string> ret;
	pivot_tree_->getHeavySubtreePathBeforeTimeStamp(vertex, stamp, ret);
	return ret;
}

time_stamp_t DagManager::getStateBlockTimeStamp(std::string const & vertex){
	return total_dag_->getVertexTimeStamp(vertex);
}

void DagManager::setStateBlockTimeStamp(std::string const & vertex, time_stamp_t stamp){
	total_dag_->setVertexTimeStamp(vertex, stamp);
	pivot_tree_->setVertexTimeStamp(vertex, stamp);
}

SbBuffer::SbBuffer(): stopped_(false), updated_(false), iter_(blocks_.end()){}

void SbBuffer::stop() { 

	stopped_ = true;
	condition_.notify_all();
}

bool SbBuffer::isStopped() const {return stopped_;}

void SbBuffer::insert(StateBlock const & blk){
	ulock lock(mutex_);
	time_point_t t = std::chrono::system_clock::now ();
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

TipBlockExplorer::TipBlockExplorer(unsigned rate): 
	ready_(false), on_(true), rate_limit_(rate), counter_(0){}


// only one DagManager will call the function
void TipBlockExplorer::blockAdded(){
	ulock lock(mutex_);
	counter_++;
	if (counter_ >= rate_limit_){
		ready_ = true;
		counter_ = 0;
		condition_.notify_one();
	}
	else{
		ready_ = false;
	}
}

TipBlockExplorer::~TipBlockExplorer(){
	on_ = false;
	condition_.notify_all();
}

void TipBlockExplorer::waitForReady(){
	ulock lock(mutex_);
	while (on_ && !ready_){
		condition_.wait(lock);
	}
	ready_ = false;
}

} // namespace taraxa