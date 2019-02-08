/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-07 14:20:33 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-08 14:04:47
 */
 
#ifndef CONCUR_HASH_HPP
#define CONCUR_HASH_HPP
#include <string>
#include <list>
#include <mutex>
#include <vector>
#include <atomic>

#include "util.hpp"

namespace taraxa{
	namespace storage{
	// transaction type
	using trx_t = std::string;

	enum class ItemStatus: uint8_t {
		read = 0, 
		shared = 1, 
		write = 2
	};

	struct Addr{
		Addr(std::string const& contract, std::string const & storage): 
			contract(contract), storage(storage){}
		std::string contract;
		std::string storage;
	};

	class Item{
	public:
		Item (Addr const &addr, trx_t const &trx, ItemStatus status_);
		std::size_t getHash() const { return std::hash<std::string> ()(addr_.contract+addr_.storage);}
		bool operator<(Item const & other) const { return getHash() < other.getHash();}
		bool operator==(Item const & other) const { return getHash() == other.getHash();}
		friend std::ostream & operator<<(std::ostream &strm, Item const & item){
			strm<<"Addr: "<<item.addr_.contract<<" "<<item.addr_.storage<<" "<<asInteger(item.status_)<<"\n";
			return strm;
		}
	private:
		Addr addr_;
		trx_t trx_;
		ItemStatus status_;
		bool dirty_;
		trx_t old_val;
	};

	/**
	 * concur_degree is exponent, determines the granularity of locks, 
	 * cannot change once ConcurHahs is constructed
	 * number of locks = 2^(concur_degree)
	 * 
	 * Note: Do resize up but not down for now
	 */
	
	template <typename Item> 
	class ConcurHash{
	public:
		using ulock = std::unique_lock<std::mutex>;
		using bucket = std::list<Item>;
		ConcurHash (unsigned concur_degree_exp);
		bool insert(Item item);
		bool has(Item item);
		bool remove(Item item) ;

		void setVerbose(bool verbose){ verbose_ = verbose;}
		std::size_t getConcurrentDegree();
		//query only if no threads is writing ...
		std::size_t getBucketSize();
		uint64_t getItemSize(); 
	
	private:

		unsigned getBucketId(Item item) const;
		unsigned getLockId(Item item) const;
		void resize(); // double buckets_ size
		bool policy() const; // determines when to do resize
		bool verbose_;
		const std::size_t concur_degree_; 
		std::atomic<uint64_t> total_size_;  // total number of items
		std::atomic<std::size_t> bucket_size_;
		std::vector<bucket> buckets_;
		std::vector<std::mutex> mutexes_; 
	};
	
	using ConflictHash = ConcurHash<Item>;

	}// namespace storage
}// namespace taraxa




#endif
