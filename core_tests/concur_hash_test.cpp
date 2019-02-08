/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-07 15:25:43 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-07 23:03:20
 */

#include <vector>
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include "concur_hash.hpp"

namespace taraxa {
	namespace storage{
		TEST(ConcurHash, hash_func){
			ConflictHash conflict_hash(2);
			std::vector<Item> items;
			//generate fake items address
			std::string contract, storage, trx;
			unsigned num_threads = 8;
			unsigned num_tasks_each_thread = 40;
			for (auto i=0; i<8*40; ++i){
				contract+='a';
				storage+='b';
				trx+='c';
				Addr addr(contract, storage);
				items.push_back({addr, trx, ItemStatus::read});
			}
	
			std::vector<boost::thread> threads;
			for (auto thread_id=0; thread_id<num_threads; ++thread_id){
				threads.push_back(boost::thread([thread_id, num_threads, num_tasks_each_thread, &items, &conflict_hash](){
					for (auto i=0; i<num_tasks_each_thread; ++i){
						conflict_hash.insert(items[i*num_threads+thread_id]);
					}
				}));
			}
			for (auto & t: threads){
				t.join();
			}
		}
		



	} // namespace storage
}// namespace taraxa


int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
