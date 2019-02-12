/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2019-02-08 15:04:16 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-11 18:29:18
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
	if (!success){ // no existence
		// insert using dummy ConflictValue
		success=hash_.compareAndSwap(key, old_value, ConflictValue(trx, ConflictStatus::read));
		// usually success
		// when can it fail? 
		// it's competing with another thread also write to the same address and lose,
		// the other thread wins. 
		// if the other one is a read, this can be non-conflict but faluty rejected ...
		// i.e., false negative
	}
	else{
		if (old_value.getTrx() == trx) { // Question, what if the status is write? Still success?
			success = true;
		}
		// address accessed by different transaction
		else {
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
	}
	return success;
}

bool Detector::store(addr_t const & contract, addr_t const & storage, trx_t const & trx){
	bool ret = false;
	ConflictKey key(contract, storage);
	ConflictValue old_value;
	bool success; 
	std::tie(old_value, success) = hash_.get(key);
	if (!success){ // no existence
		// insert using dummy ConflictValue
		success=hash_.compareAndSwap(key, old_value, ConflictValue(trx, ConflictStatus::write));
	}
	else{
		// this address has been used by other transactions, cannot write no matter its status is
		if (old_value.getTrx() != trx) { 
			success = false;
		}
		// address accessed by same transaction
		else {
			ConflictStatus old_status = old_value.getStatus();
			// why ???
			if (old_status == ConflictStatus::write){
				success = true;
			}
			else if (old_status == ConflictStatus::shared){
				success = false;
			}
			else{  // old_status == ConflictStatus::read
				success = hash_.compareAndSwap(key, old_value, ConflictValue(trx, ConflictStatus::write));
			}
		}
	}
	return success;
}

} // namespace taraxa