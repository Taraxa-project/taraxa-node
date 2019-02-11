/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-07 14:20:33 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-11 15:39:27
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
	using addr_t = std::string;
	using trx_t = std::string;

	enum class ConflictStatus: uint8_t {
		read = 0, 
		shared = 1, 
		write = 2
	};

	
	class ConflictKey{
	public:
		 
		ConflictKey (addr_t const & contract, addr_t const & storage): contract_(contract), storage_(storage){}
		std::size_t getHash() const { return std::hash<std::string> ()(contract_+storage_);}
		bool operator<(ConflictKey const & other) const { return getHash() < other.getHash();}
		bool operator==(ConflictKey const & other) const { return getHash() == other.getHash();}
		friend std::ostream & operator<<(std::ostream &strm, ConflictKey const & key){
			strm<<"ConflictKey: "<<key.contract_<<" "<<key.storage_<<std::endl; 
			return strm;
		}
	private:
		 
		addr_t contract_; 
		addr_t storage_;

	};

	class ConflictValue{
	public:
		ConflictValue(){}
		ConflictValue (trx_t const & trx, ConflictStatus status): trx_(trx), status_(status){}
		friend std::ostream & operator<<(std::ostream &strm, ConflictValue const & value){
			strm<<"ConflictValue: "<< value.trx_<<" "<< asInteger(value.status_)<<std::endl; 
			return strm;
		}
	private:
		trx_t trx_;
		ConflictStatus status_;
	};

	/**
	 * concur_degree is exponent, determines the granularity of locks, 
	 * cannot change once ConcurHahs is constructed
	 * number of locks = 2^(concur_degree)
	 * 
	 * Note: Do resize up but not down for now
	 * Note: the hash content is expected to be modified through insertOrGet
	 */
	
	template <typename Key, typename Value> 
	class ConcurHash{
	public:

		/** An Item should define
			 * key, value
			 * getHash()
			 * operator ==
			 */

		struct Item{
			Item (Key const & key, Value const &value): key(key), value(value){}
			bool operator<(Item const & other) const { return key.getHash() < other.key.getHash();}
			bool operator==(Item const & other) const { return key.getHash() == other.key.getHash();}
			Key key;
			Value value;
		};
		using ulock = std::unique_lock<std::mutex>;
		using bucket = std::list<Item>;
		ConcurHash (unsigned concur_degree_exp);
		bool insert(Key key, Value value);
		bool has(Key key);
		Value get(Key key);
		bool remove(Key key);
		void clear();

		void setVerbose(bool verbose){ verbose_ = verbose;}
		std::size_t getConcurrentDegree();
		//query only if no threads is writing ...
		std::size_t getBucketSize();
		uint64_t getItemSize(); 
	
	private:
		unsigned getBucketId(Key key) const;
		unsigned getLockId(Key key) const;
		void resize(); // double buckets_ size
		bool policy() const; // determines when to do resize
		bool verbose_;
		const std::size_t concur_degree_; 
		const std::size_t init_bucket_size_;
		std::atomic<uint64_t> total_size_;  // total number of items
		std::atomic<std::size_t> bucket_size_;
		std::vector<bucket> buckets_;
		std::vector<std::mutex> mutexes_; 
	};
	
	using ConflictHash = ConcurHash<ConflictKey, ConflictValue>;

	}// namespace storage
}// namespace taraxa




#endif
