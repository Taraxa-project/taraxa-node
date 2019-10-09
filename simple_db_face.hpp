#ifndef TARAXA_NODE_SIMPLE_DB_FACE_HPP
#define TARAXA_NODE_SIMPLE_DB_FACE_HPP

#include <boost/thread.hpp>
#include <functional>
#include "libethereum/State.h"

using h256 = dev::h256;
const h256 TEMP_GENESIS_HASH = h256{}; /* genesisHash, put h256{} for now */

class SimpleDBFace {
 public:
  using foreach_fn =
      std::function<bool(std::string const &, std::string const &)>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  // @return if key exist, return false without updating value.
  virtual bool put(const std::string &key, const std::string &value) = 0;
  virtual bool update(const std::string &key, const std::string &value) = 0;
  // @return if key not exist, return "".
  virtual std::string get(const std::string &key) = 0;
  // @return if key exist, return false without updating value.
  virtual bool put(const h256 &key, const dev::bytes &value) = 0;
  virtual bool update(const h256 &key, const dev::bytes &value) = 0;
  // @return if key not exist, return null.
  virtual dev::bytes get(const h256 &key) = 0;
  virtual bool exists(const h256 &key) = 0;
  virtual void commit() = 0;
  virtual void forEach(foreach_fn const &f) = 0;
  virtual ~SimpleDBFace() = default;

 protected:
  boost::shared_mutex shared_mutex_;
};
#endif  // TARAXA_NODE_SIMPLE_DB_FACE_HPP