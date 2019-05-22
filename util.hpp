/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-29 15:26:50
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-03-05 12:09:56
 */

#ifndef UTIL_HPP
#define UTIL_HPP

#include <boost/asio.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>
#include <unordered_set>

#include "types.hpp"
namespace taraxa {

boost::property_tree::ptree strToJson(std::string str);
// load file and convert to json doc
boost::property_tree::ptree loadJsonFile(std::string json_file_name);

struct ProcessReturn {
  enum class Result {
    PROGRESS,
    BAD_SIG,
    SEEN,
    NEG_SPEND,
    UNRECEIVABLE,  // ?
    MISS_PREV,
    MISS_SOURCE
  };
  taraxa::addr_t user_account;
};

template <typename T, typename U>
std::vector<T> asVector(boost::property_tree::ptree const &pt,
                        boost::property_tree::ptree::key_type const &key) {
  std::vector<T> v;
  for (auto &item : pt.get_child(key)) {
    v.push_back(T(item.second.get_value<U>()));
  }
  return v;
}

template <typename enumT>
constexpr inline auto asInteger(enumT const value) {
  return static_cast<typename std::underlying_type<enumT>::type>(value);
}

template <typename E, typename T>
constexpr inline
    typename std::enable_if_t<std::is_enum_v<E> && std::is_integral_v<T>, E>
    toEnum(T value) noexcept {
  return static_cast<E>(value);
}

using stream = std::basic_streambuf<uint8_t>;
using bufferstream = boost::iostreams::stream_buffer<
    boost::iostreams::basic_array_source<uint8_t>>;
using vectorstream = boost::iostreams::stream_buffer<
    boost::iostreams::back_insert_device<std::vector<uint8_t>>>;

// Read a raw byte stream the size of T and fill value
// return true if success
template <typename T>
bool read(stream &stm, T &value) {
  static_assert(std::is_standard_layout<T>::value,
                "Cannot stream read non-standard layout types");
  auto bytes(stm.sgetn(reinterpret_cast<uint8_t *>(&value), sizeof(value)));
  return bytes == sizeof(value);
}

template <typename T>
bool write(stream &stm, T const &value) {
  static_assert(std::is_standard_layout<T>::value,
                "Cannot stream write non-standard layout types");
  auto bytes(
      stm.sputn(reinterpret_cast<uint8_t const *>(&value), sizeof(value)));
  assert(bytes == sizeof(value));
  return bytes == sizeof(value);
}

void thisThreadSleepForSeconds(unsigned sec);
void thisThreadSleepForMilliSeconds(unsigned millisec);
void thisThreadSleepForMicroSeconds(unsigned microsec);

unsigned long getCurrentTimeMilliSeconds();

/**
 * simple thread_safe hash
 */

template <typename K, typename V>
class StatusTable {
 public:
  using uLock = boost::unique_lock<boost::shared_mutex>;
  using sharedLock = boost::shared_lock<boost::shared_mutex>;
  using upgradableLock = boost::upgrade_lock<boost::shared_mutex>;
  using upgradeLock = boost::upgrade_to_unique_lock<boost::shared_mutex>;

  std::pair<V, bool> get(K const &hash) {
    sharedLock lock(shared_mutex_);
    auto iter = status_.find(hash);
    if (iter != status_.end()) {
      return {iter->second, true};
    } else {
      return {V(), false};
    }
  }
  bool insert(K const &hash, V status) {
    upgradableLock lock(shared_mutex_);
    bool ret = false;
    if (status_.count(hash)) {
      ret = false;
    } else {
      upgradeLock locked(lock);
      status_[hash] = status;
      ret = true;
    }
    return ret;
  }
  void update(K const &hash, V status) {
    uLock lock(shared_mutex_);
    status_[hash] = status;
  }

 private:
  boost::shared_mutex shared_mutex_;
  std::unordered_map<K, V> status_;
};
/**
 * Observer pattern
 */

class Observer;

class Subject {
 public:
  ~Subject();
  void subscribe(std::shared_ptr<Observer> obs);
  void unsubscribe(std::shared_ptr<Observer> obs);
  void notify();

 protected:
  std::unordered_set<std::shared_ptr<Observer>> viewers_;
};

class Observer : std::enable_shared_from_this<Observer> {
 public:
  Observer(std::shared_ptr<Subject> sub);
  virtual ~Observer();
  virtual void update() = 0;

 protected:
  std::shared_ptr<Subject> subject_;
};

}  // namespace taraxa

#endif
