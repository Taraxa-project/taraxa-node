/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-07 15:25:43 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-11 11:54:21
 */

#include <vector>
#include <thread>
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include "concur_hash.hpp"

namespace taraxa {
	namespace storage{
		TEST(ConcurHash, hash_func){
			unsigned concurrent_degree_exp = 5;
			unsigned num_threads = 8;
			unsigned num_tasks_per_thread = 1234;
			ConflictHash conflict_hash(concurrent_degree_exp);
			conflict_hash.setVerbose(true);
			std::vector<ConflictItem> items;
			//generate fake items address
			std::string contract, storage, trx;
			auto init_bucket_size = conflict_hash.getBucketSize();
			for (auto i=0; i<num_threads*num_tasks_per_thread; ++i){
				contract+='a';
				storage+='b';
				trx+='c';
				Addr addr(contract, storage);
				items.push_back({addr, ConflictStatus::read});
			}
	
			// write to hash_set
			{
				std::vector<boost::thread> threads;
				for (auto thread_id=0; thread_id<num_threads; ++thread_id){
					threads.push_back(boost::thread([thread_id, num_threads, 
					num_tasks_per_thread, &items, &conflict_hash](){
						for (auto i=0; i<num_tasks_per_thread; ++i){
							conflict_hash.insert(items[i*num_threads+thread_id]);
						}
					}));
				}
				for (auto & t: threads){
					t.join();
				}
			}
			// read from hash_set
			{
				std::vector<std::thread> threads;
				for (auto thread_id=0; thread_id<num_threads; ++thread_id){
					threads.push_back(std::thread([thread_id, num_threads, 
					num_tasks_per_thread, &items,&conflict_hash]{
						for (auto i=0; i<num_tasks_per_thread; ++i){
							bool ret = conflict_hash.has(items[i*num_threads+thread_id]);
							EXPECT_EQ(ret, true);
						}
					}));
				}
				for (auto & t: threads){
					t.join();
				}
				EXPECT_EQ(conflict_hash.getItemSize(),num_threads*num_tasks_per_thread);
				EXPECT_EQ(conflict_hash.getBucketSize(), 4096);
				EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1<<concurrent_degree_exp);

			}
			
			// remove half of hash_set
			{
				std::vector<std::thread> threads;
				for (auto thread_id=0; thread_id<num_threads/2; ++thread_id){
					threads.push_back(std::thread([thread_id, num_threads, 
					num_tasks_per_thread, &items,&conflict_hash]{
						for (auto i=0; i<num_tasks_per_thread; ++i){
							bool ret = conflict_hash.remove(items[i*num_threads+thread_id]);
							EXPECT_EQ(ret, true);
						}
					}));
				}
				for (auto & t: threads){
					t.join();
				}
				EXPECT_EQ(conflict_hash.getItemSize(), num_threads*num_tasks_per_thread/2);
				EXPECT_EQ(conflict_hash.getBucketSize(), 4096);
				EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1<<concurrent_degree_exp);
			}
			
			// clear hash_set, size reduced to initial
			{
				conflict_hash.clear();
				EXPECT_EQ(conflict_hash.getItemSize(), 0);
				EXPECT_EQ(conflict_hash.getBucketSize(), init_bucket_size);
				EXPECT_EQ(conflict_hash.getConcurrentDegree(), 1<<concurrent_degree_exp);
			}

		}
		

		
	} // namespace storage
}// namespace taraxa


int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
