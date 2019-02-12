/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-08 15:04:16 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-12 12:44:04
 */
 
#include "conflict_detector.hpp"
namespace taraxa{

Detector::Detector(unsigned degree_exp): hash_(degree_exp){}

bool Detector::load(addr_t const & contract, addr_t const & storage, trx_t const & trx){
	bool ret = false;
	ConflictKey key(contract, storage);
	ConflictValue old_value;
	bool success; 
	std::tie(old_value, success) = hash_.get(key);
	
	// no existence
	if (!success){
		// insert using dummy ConflictValue
		success=hash_.compareAndSwap(key, old_value, ConflictValue(trx, ConflictStatus::read));
		// usually success
		// when can it fail? 
		// it's competing with another thread (transaction) also write to the same address but lose,
		// the other thread wins. 
		// if the other one is a read, this can be non-conflict but faluty rejected ...
		// i.e., false negative
	}
	// the address is accessed by same transaction
	else if (old_value.getTrx() == trx){
		success = true;
	}
	// address already accessed by different transaction
	else{
		
		ConflictStatus old_status = old_value.getStatus();
		if (old_status == ConflictStatus::shared){
			success = true;
		}
		else if (old_status == ConflictStatus::write){
			success = false;
		}
		else{  // old_status == ConflictStatus::read
			success = hash_.compareAndSwap(key, old_value, ConflictValue(trx, ConflictStatus::shared));
			// success is false if another thread is competing
			// can be false negative if new status is shared
		} 
	}
	return success;
}

bool Detector::store(addr_t const & contract, addr_t const & storage, trx_t const & trx){
	bool ret = false;
	ConflictKey key(contract, storage);
	ConflictValue old_value;
	bool success; 
	std::tie(old_value, success) = hash_.get(key);
	// no existence
	if (!success){ 
		// insert using dummy ConflictValue
		success=hash_.compareAndSwap(key, old_value, ConflictValue(trx, ConflictStatus::write));
	}
	// the address has been accessed by other transaction
	else if (old_value.getTrx() != trx) { 
		success = false;
	}

	// address accessed by same transaction
	else{ 
		ConflictStatus old_status = old_value.getStatus();
		// the address has been written by "same transaction", such as a loop to write same address
		if (old_status == ConflictStatus::write){
			success = true;
		}
		// the address has been shared with more transactions, but the first transaction is itself
		else if (old_status == ConflictStatus::shared){
			success = false;
		}
		else{  // old_status == ConflictStatus::read
			success = hash_.compareAndSwap(key, old_value, ConflictValue(trx, ConflictStatus::write));
		}
		 
	}
	return success;
}

} // namespace taraxa