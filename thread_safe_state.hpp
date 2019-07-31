#ifndef TARAXA_NODE__THREAD_SAFE_STATE_HPP_
#define TARAXA_NODE__THREAD_SAFE_STATE_HPP_

#include <libethereum/State.h>
#include <mutex>
#include "types.hpp"

namespace taraxa::thread_safe_state {
using namespace dev;
using namespace std;

class ThreadSafeState : public eth::State {
 protected:
  mutex mutable m_;

 public:
  using eth::State::State;

  ThreadSafeState(ThreadSafeState const& s) : m_(), eth::State(s) {}

  ThreadSafeState& operator=(ThreadSafeState const& s) {
    unique_lock l(m_);
    eth::State::operator=(s);
    return *this;
  }

  OverlayDB& db() {
    unique_lock l(m_);
    return eth::State::db();
  }

  const OverlayDB& db() const {
    unique_lock l(m_);
    return eth::State::db();
  }

  void populateFrom(eth::AccountMap const& account_map) {
    unique_lock l(m_);
    eth::State::populateFrom(account_map);
  }

  void commit(CommitBehaviour commit_behaviour) {
    unique_lock l(m_);
    eth::State::commit(commit_behaviour);
  }

  void setRoot(root_t const& root) {
    unique_lock l(m_);
    eth::State::setRoot(root);
  }

  std::unordered_map<Address, u256> addresses() const {
    unique_lock l(m_);
    return eth::State::addresses();
  }

  std::pair<AddressMap, h256> addresses(h256 const& _begin,
                                        size_t _maxResults) const {
    unique_lock l(m_);
    return eth::State::addresses(_begin, _maxResults);
  }

  bool addressInUse(Address const& _address) const {
    unique_lock l(m_);
    return eth::State::addressInUse(_address);
  }

  bool accountNonemptyAndExisting(Address const& _address) const {
    unique_lock l(m_);
    return eth::State::accountNonemptyAndExisting(_address);
  }

  bool addressHasCode(Address const& _address) const {
    unique_lock l(m_);
    return eth::State::addressHasCode(_address);
  }

  u256 balance(Address const& _id) const {
    unique_lock l(m_);
    return eth::State::balance(_id);
  }

  void addBalance(Address const& _id, val_t const& _amount) {
    unique_lock l(m_);
    eth::State::addBalance(_id, _amount);
  }

  void subBalance(Address const& _addr, val_t const& _value) {
    unique_lock l(m_);
    eth::State::subBalance(_addr, _value);
  }

  void setBalance(Address const& _addr, val_t const& _value) {
    unique_lock l(m_);
    eth::State::setBalance(_addr, _value);
  }

  void transferBalance(Address const& _from, Address const& _to,
                       val_t const& _value) {
    unique_lock l(m_);
    eth::State::transferBalance(_from, _to, _value);
  }

  h256 storageRoot(Address const& _contract) const {
    unique_lock l(m_);
    return eth::State::storageRoot(_contract);
  }

  u256 storage(Address const& _contract, u256 const& _memory) const {
    unique_lock l(m_);
    return eth::State::storage(_contract, _memory);
  }

  void setStorage(Address const& _contract, u256 const& _location,
                  u256 const& _value) {
    unique_lock l(m_);
    eth::State::setStorage(_contract, _location, _value);
  }

  u256 originalStorageValue(Address const& _contract, u256 const& _key) const {
    unique_lock l(m_);
    return eth::State::originalStorageValue(_contract, _key);
  }

  void clearStorage(Address const& _contract) {
    unique_lock l(m_);
    eth::State::clearStorage(_contract);
  }

  void createContract(Address const& _address) {
    unique_lock l(m_);
    eth::State::createContract(_address);
  }

  void setCode(Address const& _address, bytes&& _code) {
    unique_lock l(m_);
    eth::State::setCode(_address, std::move(_code));
  }

  void kill(Address _a) {
    unique_lock l(m_);
    eth::State::kill(_a);
  }

  std::map<h256, std::pair<u256, u256>> storage(
      Address const& _contract) const {
    unique_lock l(m_);
    return eth::State::storage(_contract);
  }

  const bytes& code(Address const& _addr) const {
    unique_lock l(m_);
    return eth::State::code(_addr);
  }

  h256 codeHash(Address const& _contract) const {
    unique_lock l(m_);
    return eth::State::codeHash(_contract);
  }

  size_t codeSize(Address const& _contract) const {
    unique_lock l(m_);
    return eth::State::codeSize(_contract);
  }

  void incNonce(Address const& _id) {
    unique_lock l(m_);
    eth::State::incNonce(_id);
  }

  void setNonce(Address const& _addr, u256 const& _newNonce) {
    unique_lock l(m_);
    eth::State::setNonce(_addr, _newNonce);
  }

  u256 getNonce(Address const& _addr) const {
    unique_lock l(m_);
    return eth::State::getNonce(_addr);
  }

  h256 rootHash() const {
    unique_lock l(m_);
    return eth::State::rootHash();
  }

  const u256& accountStartNonce() const {
    unique_lock l(m_);
    return eth::State::accountStartNonce();
  }

  const u256& requireAccountStartNonce() const {
    unique_lock l(m_);
    return eth::State::requireAccountStartNonce();
  }

  void noteAccountStartNonce(u256 const& _actual) {
    unique_lock l(m_);
    eth::State::noteAccountStartNonce(_actual);
  }

  size_t savepoint() const {
    unique_lock l(m_);
    return eth::State::savepoint();
  }

  void rollback(size_t _savepoint) {
    unique_lock l(m_);
    eth::State::rollback(_savepoint);
  }

  const eth::ChangeLog& changeLog() const {
    unique_lock l(m_);
    return eth::State::changeLog();
  }
};

}  // namespace taraxa::thread_safe_state

namespace taraxa {
using thread_safe_state::ThreadSafeState;
}

#endif  // TARAXA_NODE__THREAD_SAFE_STATE_HPP_
