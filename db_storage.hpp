#ifndef TARAXA_NODE_DB_STORAGE
#define TARAXA_NODE_DB_STORAGE

#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <boost/filesystem.hpp>
#include "dag_block.hpp"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "util.hpp"

namespace taraxa {
using namespace std;
using namespace dev;
using namespace rocksdb;
namespace fs = boost::filesystem;

class DbException : public exception {
 public:
  DbException(string desc) : desc_(desc) {}
  virtual ~DbException() {}
  virtual const char* what() const noexcept { return desc_.c_str(); }

 private:
  string desc_;
};

class DbStorage {
 public:
  enum Columns : unsigned char {
    default_column = 0,
    dag_blocks,
    dag_blocks_index,
    column_end
  };

  DbStorage(fs::path const& base_path, h256 const& genesis_hash,
            bool drop_existing);

  ~DbStorage() {
    for (auto handle : handles_) delete handle;
  }
  void checkStatus(rocksdb::Status& _status);
  std::string lookup(Slice _key, Columns column);

  std::shared_ptr<DagBlock> getDagBlock(blk_hash_t const& hash);
  std::string getBlocksByLevel(level_t level);
  void saveDagBlock(DagBlock const& blk);

  inline Slice toSlice(dev::bytes const& _b) const {
    return Slice(reinterpret_cast<char const*>(&_b[0]), _b.size());
  }

  template <class N, typename = std::enable_if_t<std::is_arithmetic<N>::value>>
  inline Slice toSlice(N const& n) {
    return Slice(reinterpret_cast<char const*>(&n), sizeof(N));
  }

  inline Slice toSlice(string const& _str) {
    return Slice(_str.data(), _str.size());
  }

  inline bytes asBytes(std::string const& _b) {
    return bytes((byte const*)_b.data(), (byte const*)(_b.data() + _b.size()));
  }

 private:
  std::unique_ptr<DB> db_;
  std::vector<ColumnFamilyHandle*> handles_;
  ReadOptions read_options_;
  WriteOptions write_options_;
  static const constexpr char* const column_names[] = {"default", "dag_blocks",
                                                       "dag_blocks_index"};
};
}  // namespace taraxa
#endif
