/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-07 14:20:45 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-08 00:34:54
 */

#include <iostream>
#include <functional> 
#include <algorithm>
#include "concur_hash.hpp"

namespace taraxa{
	namespace storage{
		
	Item::Item(Addr const &addr, trx_t const &trx, ItemStatus status): 
		addr_(addr), trx_(trx), status_(status), dirty_(false){}

	template <typename Item>
	ConcurHash<Item>::ConcurHash(unsigned concur_degree_exp): 
	concur_degree_(concur_degree_exp<<1), 
	total_size_(0),
	bucket_size_(concur_degree_),
	buckets_(concur_degree_), 
	mutexes_(concur_degree_){
		// not sure if we can have too many mutex
		assert(concur_degree_exp>=0 && concur_degree_exp<2048);
	}

	template <typename Item>
	bool ConcurHash<Item>::insert(Item item){
		bool res = false;
		{
			ulock lock(mutexes_[getLockId(item)]);
			unsigned bucket_id = getBucketId(item);
			auto & bucket = buckets_[bucket_id];
			auto it = std::find(bucket.begin(), bucket.end(), item);
			if (it != bucket.end()){ // item exist
				res = false;
			}
			else {
				bucket.insert(item);
				total_size_++;
				res = true;
			}
		}
		if (res && policy()){
			resize();
		}
		return res;
	}

	template <typename Item>
	bool ConcurHash<Item>::has(Item item) {
		bool ret = false;
		ulock lock(mutexes_[getLockId(item)]);
		unsigned bucket_id = getBucketId(item);
		auto & bucket = buckets_[bucket_id];
		auto it = std::find(bucket.begin(), bucket.end(), item);
		if (it != bucket.end()){ // item exist
			ret = true;
		}
		return ret;
	}

	template <typename Item>
	bool ConcurHash<Item>::remove(Item item){
		return true;
	}
	
	template <typename Item>
	unsigned ConcurHash<Item>::getBucketId(Item item) const{
		return item.getHash()%bucket_size_;
	}

	template <typename Item>
	unsigned ConcurHash<Item>::getLockId(Item item) const{
		return item.getHash()%mutexes_.size();
	}
	
	template <typename Item> 
	bool ConcurHash<Item>::policy() const{
		bool ret=false;
		if (total_size_ >= buckets_.size()*4){
			ret = true;
		}
		return ret;
	}

	template <typename Item> 
	void ConcurHash<Item>::resize(){
		auto old_size = buckets_.size();

		std::vector<ulock> locks(mutexes_.begin(), mutexes_.end());

		if (buckets_.size() != old_size){
			return;
		}
		// std::cout<<"Start resizing: old_size = "<<old_size<<std::endl;
		buckets_.resize(old_size*2);
		bucket_size_=buckets_.size();
		// std::cout<<"Resized bucket size = "<<buckets_.size()<<std::endl;
		// rehash 
		for (auto i=0; i<old_size; ++i){
			auto & current_bucket = buckets_[i];
			for (auto iter=current_bucket.begin(); iter!=current_bucket.end();){
				auto new_bucket_id = getBucketId(*iter);
				if (new_bucket_id != i){ // split, move this to new bucket
					// std::cout<<"Resize: move old_bucket_id "<<i<< " to new_bucket_id "<<new_bucket_id<<std::endl;
					buckets_[new_bucket_id].insert(*iter);
					assert(new_bucket_id == i + old_size);
					iter=current_bucket.erase(iter);
				}
				else{
					iter++;
				}
			}
		}
	}
	// create instance
	template class ConcurHash<Item>;
	}// namespace storage
}// namespace taraxa
