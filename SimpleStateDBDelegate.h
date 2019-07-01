/*
 * @Copyright: Taraxa.io
 * @Author: JC
 * @Date: 2019-04-28 15:45:35
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-05-15 16:12:10
 */

#ifndef TARAXA_NODE_SIMPLESTATEDBDELEGATE_H
#define TARAXA_NODE_SIMPLESTATEDBDELEGATE_H
#include <libdevcore/DBFactory.h>
#include <boost/filesystem.hpp>
#include "SimpleDBFace.h"
#include "util_eth.hpp"

class SimpleStateDBDelegate : public SimpleDBFace {
  const std::shared_ptr<dev::eth::State> state_;
  const taraxa::util::eth::RawDBAndMeta raw_db_;

  explicit SimpleStateDBDelegate(const taraxa::util::eth::OverlayAndRawDB &);

  template <typename Lock>
  struct LockedState {
    std::shared_ptr<dev::eth::State> state;
    Lock lock;

    void release() {
      state = nullptr;
      lock.unlock();
    }
    const auto &operator-> () const { return state; }
  };

 public:
  bool put(const std::string &key, const std::string &value) override;
  bool update(const std::string &key, const std::string &value) override;
  std::string get(const std::string &key) override;
  void commit() override;

  template <template <class> class Lock>
  LockedState<Lock<decltype(shared_mutex_)>> getState() {
    return {state_, Lock(shared_mutex_)};
  }

  //  const auto &getState() { return state_; }

  const auto &getRawDB() const { return raw_db_; }

  SimpleStateDBDelegate(const std::string &path, bool overwrite);

 private:
  static dev::Address stringToAddress(const std::string &s) {
    return dev::Address(s);
  }
  static taraxa::bal_t stringToBalance(const std::string &s) {
    return (taraxa::bal_t)std::stoull(s);
  }
};
#endif  // TARAXA_NODE_SIMPLESTATEDBDELEGATE_H
