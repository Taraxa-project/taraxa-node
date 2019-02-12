/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-07 14:20:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-11 16:20:18
 */

#include <iostream>
#include <functional> 
#include <algorithm>
#include "concur_hash.hpp"

namespace taraxa{
	namespace storage{

	template <typename Key, typename Value>
	ConcurHash<Key, Value>::ConcurHash(unsigned concur_degree_exp): 
	verbose_(false),
	concur_degree_(1<<concur_degree_exp), 
	init_bucket_size_(concur_degree_*4),
	total_size_(0),
	bucket_size_(init_bucket_size_),
	buckets_(bucket_size_), 
	mutexes_(concur_degree_){
		// not sure if we can have too many mutex
		assert(concur_degree_exp>=0 && concur_degree_exp<10);
	}

	template <typename Key, typename Value>
	bool ConcurHash<Key, Value>::insert(Key key, Value value){
		bool res = false;
		{
			ulock lock(mutexes_[getLockId(key)]);
			unsigned bucket_id = getBucketId(key);
			auto & bucket = buckets_[bucket_id];
			Item item(key, value);
			auto it = std::find(bucket.begin(), bucket.end(), item);
			if (it != bucket.end()){ // item exist
				res = false;
			}
			else {
				bucket.emplace_back(item);
				total_size_.fetch_add(1);
				res = true;
			}
		}
		if (res && policy()){
			resize();
		}
		return res;
	}

	template <typename Key, typename Value>
	bool ConcurHash<Key, Value>::compareAndSwap(Key key, Value expected_value, Value new_value){
		bool success = false;
		ulock lock(mutexes_[getLockId(key)]);
		auto bucket_id = getBucketId(key);
		auto & bucket = buckets_[bucket_id];
		Value old_value; 
		Item item(key, Value());
		auto it = std::find(bucket.begin(), bucket.end(), item);
		if (it != bucket.end()){ // item exist
			old_value = it->value;
			if (old_value == expected_value){ 
				it->value = new_value;
				success = true;
			}
		} 
		return success;
	}

	template <typename Key, typename Value>
	std::pair<Value, bool> ConcurHash<Key, Value>::get(Key key) {
		bool success = false;
		ulock lock(mutexes_[getLockId(key)]);
		auto bucket_id = getBucketId(key);
		auto & bucket = buckets_[bucket_id];
		Value ret;
		Item item(key, Value());
		auto it = std::find(bucket.begin(), bucket.end(), item);
		if (it != bucket.end()){ // item exist
			success = true;
			ret = it->value;
		}
		return {ret, success};
	}

	template <typename Key, typename Value>
	bool ConcurHash<Key, Value>::remove(Key key){
		bool res = false;
		{
			ulock lock(mutexes_[getLockId(key)]);
			unsigned bucket_id = getBucketId(key);
			auto & bucket = buckets_[bucket_id];
			Value dummy;
			Item item(key, dummy);
			auto it = std::find(bucket.begin(), bucket.end(), item);
			if (it == bucket.end()){ // item not find
				res = false;
			}
			else {
				bucket.erase(it);
				total_size_.fetch_add(-1);
				res = true;
			}
		}
		return true;
	}
	
	template <typename Key, typename Value>
	unsigned ConcurHash<Key, Value>::getBucketId(Key key) const{
		return key.getHash()%bucket_size_;
	}

	template <typename Key, typename Value>
	unsigned ConcurHash<Key, Value>::getLockId(Key key) const{
		return key.getHash()%mutexes_.size();
	}
	
	template <typename Key, typename Value>
	bool ConcurHash<Key, Value>::policy() const{
		bool ret=false;
		if (total_size_ >= buckets_.size()*4){
			ret = true;
		}
		return ret;
	}

	template <typename Key, typename Value>
	void ConcurHash<Key, Value>::resize(){
		auto old_size = buckets_.size();

		std::vector<ulock> locks(mutexes_.begin(), mutexes_.end());

		if (buckets_.size() != old_size){
			return;
		}
		// std::cout<<"Start resizing: old_size = "<<old_size<<std::endl;
		buckets_.resize(old_size*2);
		bucket_size_.store(buckets_.size());
		if (verbose_){
			std::cout<<"Resized bucket size = "<<bucket_size_<<std::endl;
		}
		// rehash 
		for (auto i=0; i<old_size; ++i){
			auto & current_bucket = buckets_[i];
			for (auto iter=current_bucket.begin(); iter!=current_bucket.end();){
				auto new_bucket_id = getBucketId(iter->key);
				if (new_bucket_id != i){ // split, move this to new bucket
					// std::cout<<"Resize: move old_bucket_id "<<i<< " to new_bucket_id "<<new_bucket_id<<std::endl;
					// TODO: can be optimized ...
					buckets_[new_bucket_id].emplace_back(*iter);
					assert(new_bucket_id == i + old_size);
					iter=current_bucket.erase(iter);
				}
				else{
					iter++;
				}
			}
		}
	}

	template <typename Key, typename Value>
	std::size_t ConcurHash<Key, Value>::getBucketSize(){
		//std::vector<ulock> locks(mutexes_.begin(), mutexes_.end());
		return bucket_size_;
	}
	
	template <typename Key, typename Value>
	uint64_t ConcurHash<Key, Value>::getItemSize(){
		//std::vector<ulock> locks(mutexes_.begin(), mutexes_.end());
		return total_size_;
	} 

	template <typename Key, typename Value>
	std::size_t ConcurHash<Key, Value>::getConcurrentDegree(){
		return concur_degree_;
	}

	template <typename Key, typename Value>
	void ConcurHash<Key, Value>::clear(){

		auto old_size = buckets_.size();
		std::vector<ulock> locks(mutexes_.begin(), mutexes_.end());
		
		if (buckets_.size() != old_size){
			return;
		}
		
		for (auto & bucket: buckets_){
			bucket.clear();
		}
		
		buckets_.resize(init_bucket_size_);
		bucket_size_.store(buckets_.size());
		total_size_.store(0);
	}

	// create instance
	template class ConcurHash<ConflictKey, ConflictValue>;
	}// namespace storage
}// namespace taraxa
