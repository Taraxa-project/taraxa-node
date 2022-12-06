#include "final_chain/cache.hpp"

#include <optional>

#include "test_util/gtest.hpp"

namespace taraxa::final_chain {

struct CacheTest : WithDataDir {};

// Cache for testing. Value is equal to block number(key == value)
class ValueCacheTestable : public ValueByBlockCache<uint64_t> {
 public:
  ValueCacheTestable(uint64_t limit) : ValueByBlockCache<uint64_t>(limit, [](uint64_t a) { return a; }) {}
  uint64_t blocksSize() { return data_by_block_.size(); }

  bool haveBlock(uint64_t block) { return data_by_block_.contains(block); }
};

class MapCacheTestable : public MapByBlockCache<uint64_t, uint64_t> {
 public:
  MapCacheTestable(uint64_t limit)
      : MapByBlockCache<uint64_t, uint64_t>(limit, [](uint64_t a, uint64_t) { return a; }) {}
  uint64_t blocksSize() { return data_by_block_.size(); }

  bool haveBlock(uint64_t block) { return data_by_block_.contains(block); }

  bool contains(uint64_t block, uint64_t key) {
    auto block_data = data_by_block_.find(block);
    if (block_data != data_by_block_.end()) {
      return block_data->second.contains(key);
    }
    return false;
  }
};

TEST_F(CacheTest, value_caching) {
  ValueCacheTestable cache(1);
  EXPECT_EQ(cache.blocksSize(), 0);

  EXPECT_EQ(cache.get(1), 1);
  EXPECT_TRUE(cache.haveBlock(1));
  EXPECT_EQ(cache.blocksSize(), 1);
}

TEST_F(CacheTest, value_overwrite) {
  ValueCacheTestable cache(3);

  EXPECT_EQ(cache.get(1), 1);
  EXPECT_EQ(cache.get(2), 2);
  EXPECT_EQ(cache.get(3), 3);
  EXPECT_TRUE(cache.haveBlock(1));
  EXPECT_TRUE(cache.haveBlock(2));
  EXPECT_TRUE(cache.haveBlock(3));
  EXPECT_EQ(cache.blocksSize(), 3);

  EXPECT_EQ(cache.get(4), 4);
  EXPECT_FALSE(cache.haveBlock(1));
  EXPECT_TRUE(cache.haveBlock(2));
  EXPECT_EQ(cache.blocksSize(), 3);

  // Should return correct value, but not save it because it is too old
  EXPECT_EQ(cache.get(1), 1);
  EXPECT_FALSE(cache.haveBlock(1));
  EXPECT_TRUE(cache.haveBlock(2));
  EXPECT_EQ(cache.blocksSize(), 3);

  EXPECT_EQ(cache.get(5), 5);
  EXPECT_FALSE(cache.haveBlock(2));
  EXPECT_TRUE(cache.haveBlock(3));
  EXPECT_EQ(cache.blocksSize(), 3);

  EXPECT_EQ(cache.get(10), 10);
  EXPECT_EQ(cache.blocksSize(), 3);
}

TEST_F(CacheTest, map_caching) {
  MapCacheTestable cache(1);
  EXPECT_EQ(cache.blocksSize(), 0);

  EXPECT_EQ(cache.get(1, 1), 1);
  EXPECT_TRUE(cache.haveBlock(1));
  EXPECT_EQ(cache.blocksSize(), 1);
  EXPECT_TRUE(cache.contains(1, 1));
  EXPECT_FALSE(cache.contains(1, 2));

  EXPECT_EQ(cache.get(1, 2), 1);
  EXPECT_TRUE(cache.haveBlock(1));
  EXPECT_EQ(cache.blocksSize(), 1);
  EXPECT_TRUE(cache.contains(1, 1));
  EXPECT_TRUE(cache.contains(1, 2));
}

TEST_F(CacheTest, map_overwrite) {
  MapCacheTestable cache(3);

  EXPECT_EQ(cache.get(1, 1), 1);
  EXPECT_EQ(cache.get(2, 2), 2);
  EXPECT_EQ(cache.get(3, 3), 3);
  EXPECT_TRUE(cache.haveBlock(1));
  EXPECT_TRUE(cache.haveBlock(2));
  EXPECT_TRUE(cache.haveBlock(3));
  EXPECT_EQ(cache.blocksSize(), 3);

  EXPECT_EQ(cache.get(4, 4), 4);
  EXPECT_FALSE(cache.haveBlock(1));
  EXPECT_TRUE(cache.haveBlock(2));
  EXPECT_EQ(cache.blocksSize(), 3);

  // Should return correct value, but not save it because it is too old
  EXPECT_EQ(cache.get(1, 1), 1);
  EXPECT_FALSE(cache.haveBlock(1));
  EXPECT_TRUE(cache.haveBlock(2));
  EXPECT_EQ(cache.blocksSize(), 3);

  EXPECT_EQ(cache.get(5, 5), 5);
  EXPECT_FALSE(cache.haveBlock(2));
  EXPECT_TRUE(cache.haveBlock(3));
  EXPECT_EQ(cache.blocksSize(), 3);

  EXPECT_EQ(cache.get(10, 10), 10);
  EXPECT_EQ(cache.blocksSize(), 3);
}

}  // namespace taraxa::final_chain

TARAXA_TEST_MAIN({})
