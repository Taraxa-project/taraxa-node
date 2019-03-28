/*
 This file is a modified GoogleTest compatible version of cpp-ethereum unit-test for Trie.
 */

#include <gtest/gtest.h>
#include "libdevcore/Log.h"

#include <fstream>
#include <json_spirit/JsonSpiritHeaders.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/TrieDB.h>
#include <libdevcore/TrieHash.h>
#include <libdevcore/StateCacheDB.h>
#include "MemTrie.h"
#include <boost/filesystem/path.hpp>

using namespace std;
using namespace dev;

namespace fs = boost::filesystem;
namespace js = json_spirit;

static unsigned fac(unsigned _i)
{
    return _i > 2 ? _i * fac(_i - 1) : _i;
}

using dev::operator <<;

namespace taraxa {
    // disable until we want to use fat_trie
    TEST(Trie, DISABLED_fat_trie)
    {
        h256 r;
        StateCacheDB fm;
        {
            FatGenericTrieDB<StateCacheDB> ft(&fm);
            ft.init();
            ft.insert(h256("69", h256::FromHex, h256::AlignRight).ref(), h256("414243", h256::FromHex, h256::AlignRight).ref());
            for (auto i: ft)
                cnote << i.first << i.second;
            r = ft.root();
        }
        {
            FatGenericTrieDB<StateCacheDB> ft(&fm);
            ft.setRoot(r);
            for (auto i: ft)
                cnote << i.first << i.second;
        }
    }

    testing::AssertionResult sLengthCheck (const string& s, string message)
    {
        if(s.length() > 0)
            return ::testing::AssertionSuccess();
        else
            return ::testing::AssertionFailure()
            << message;
    }
    
    /*
     Set the testPath to test_build directory which will be cleaned
     */
    boost::filesystem::path getTestPath()
    {
        return boost::filesystem::path("test_build");
    }
    
    //TODO: fix the broken-generation-process for test json file
    TEST(Trie, DISABLED_hex_encoded_securetrie_test)
    {
        fs::path const testPath = getTestPath() / fs::path("TrieTests");
        
        cnote << "Testing Secure Trie...";
        js::mValue v;
        string const s = contentsString(testPath / fs::path("hex_encoded_securetrie_test.json"));
        EXPECT_TRUE(sLengthCheck(s, "Contents of 'hex_encoded_securetrie_test.json' is empty. Have you cloned the 'tests' repo branch develop?"));
        js::read_string(s, v);
        for (auto& i: v.get_obj())
        {
            cnote << i.first;
            js::mObject& o = i.second.get_obj();
            vector<pair<string, string>> ss;
            for (auto i: o["in"].get_obj())
            {
                ss.push_back(make_pair(i.first, i.second.get_str()));
                if (!ss.back().first.find("0x"))
                    ss.back().first = asString(fromHex(ss.back().first.substr(2)));
                if (!ss.back().second.find("0x"))
                    ss.back().second = asString(fromHex(ss.back().second.substr(2)));
            }
            for (unsigned j = 0; j < min(1000000000u, fac((unsigned)ss.size())); ++j)
            {
                next_permutation(ss.begin(), ss.end());
                StateCacheDB m;
                EnforceRefs r(m, true);
                GenericTrieDB<StateCacheDB> t(&m);
                StateCacheDB hm;
                EnforceRefs hr(hm, true);
                HashedGenericTrieDB<StateCacheDB> ht(&hm);
                StateCacheDB fm;
                EnforceRefs fr(fm, true);
                FatGenericTrieDB<StateCacheDB> ft(&fm);
                t.init();
                ht.init();
                ft.init();
                EXPECT_TRUE(t.check(true));
                EXPECT_TRUE(ht.check(true));
                EXPECT_TRUE(ft.check(true));
                for (auto const& k: ss)
                {
                    t.insert(k.first, k.second);
                    ht.insert(k.first, k.second);
                    ft.insert(k.first, k.second);
                    EXPECT_TRUE(t.check(true));
                    EXPECT_TRUE(ht.check(true));
                    EXPECT_TRUE(ft.check(true));
                    auto i = ft.begin();
                    auto j = t.begin();
                    for (; i != ft.end() && j != t.end(); ++i, ++j)
                    {
                        EXPECT_EQ(i == ft.end(), j == t.end());
                        EXPECT_TRUE((*i).first.toBytes() == (*j).first.toBytes());
                        EXPECT_TRUE((*i).second.toBytes() == (*j).second.toBytes());
                    }
                    EXPECT_EQ(ht.root(), ft.root());
                }
                EXPECT_TRUE(!o["root"].is_null());
                EXPECT_EQ(o["root"].get_str(), toHexPrefixed(ht.root().asArray()));
                EXPECT_EQ(o["root"].get_str(), toHexPrefixed(ft.root().asArray()));
            }
        }
    }
    
    //TODO: fix the broken-generation-process for test json file
    TEST(Trie, DISABLED_trie_test_anyorder)
    {
        fs::path const testPath = getTestPath() / fs::path("TrieTests");
        
        cnote << "Testing Trie...";
        js::mValue v;
        string const s = contentsString(testPath / fs::path("trieanyorder.json"));
        EXPECT_TRUE(sLengthCheck(s, "Contents of 'trieanyorder.json' is empty. Have you cloned the 'tests' repo branch develop?"));
        js::read_string(s, v);
        for (auto& i: v.get_obj())
        {
            cnote << i.first;
            js::mObject& o = i.second.get_obj();
            vector<pair<string, string>> ss;
            for (auto i: o["in"].get_obj())
            {
                ss.push_back(make_pair(i.first, i.second.get_str()));
                if (!ss.back().first.find("0x"))
                    ss.back().first = asString(fromHex(ss.back().first.substr(2)));
                if (!ss.back().second.find("0x"))
                    ss.back().second = asString(fromHex(ss.back().second.substr(2)));
            }
            for (unsigned j = 0; j < min(1000u, fac((unsigned)ss.size())); ++j)
            {
                next_permutation(ss.begin(), ss.end());
                StateCacheDB m;
                EnforceRefs r(m, true);
                GenericTrieDB<StateCacheDB> t(&m);
                StateCacheDB hm;
                EnforceRefs hr(hm, true);
                HashedGenericTrieDB<StateCacheDB> ht(&hm);
                StateCacheDB fm;
                EnforceRefs fr(fm, true);
                FatGenericTrieDB<StateCacheDB> ft(&fm);
                t.init();
                ht.init();
                ft.init();
                EXPECT_TRUE(t.check(true));
                EXPECT_TRUE(ht.check(true));
                EXPECT_TRUE(ft.check(true));
                for (auto const& k: ss)
                {
                    t.insert(k.first, k.second);
                    ht.insert(k.first, k.second);
                    ft.insert(k.first, k.second);
                    EXPECT_TRUE(t.check(true));
                    EXPECT_TRUE(ht.check(true));
                    EXPECT_TRUE(ft.check(true));
                    auto i = ft.begin();
                    auto j = t.begin();
                    for (; i != ft.end() && j != t.end(); ++i, ++j)
                    {
                        EXPECT_EQ(i == ft.end(), j == t.end());
                        EXPECT_TRUE((*i).first.toBytes() == (*j).first.toBytes());
                        EXPECT_TRUE((*i).second.toBytes() == (*j).second.toBytes());
                    }
                    EXPECT_EQ(ht.root(), ft.root());
                }
                EXPECT_TRUE(!o["root"].is_null());
                EXPECT_EQ(o["root"].get_str(), toHexPrefixed(t.root().asArray()));
                EXPECT_EQ(ht.root(), ft.root());
            }
        }
    }
    
    //TODO: fix the broken-generation-process for test json file
    TEST(Trie, DISABLED_trie_tests_ordered)
    {
        fs::path const testPath = getTestPath() / fs::path("TrieTests");
        
        cnote << "Testing Trie...";
        js::mValue v;
        string const s = contentsString(testPath / fs::path("trietest.json"));
        EXPECT_TRUE(sLengthCheck(s, "Contents of 'trietest.json' is empty. Have you cloned the 'tests' repo branch develop?"));
        js::read_string(s, v);
        
        for (auto& i: v.get_obj())
        {
            cnote << i.first;
            js::mObject& o = i.second.get_obj();
            vector<pair<string, string>> ss;
            vector<string> keysToBeDeleted;
            for (auto& i: o["in"].get_array())
            {
                vector<string> values;
                for (auto& s: i.get_array())
                {
                    if (s.type() == json_spirit::str_type)
                        values.push_back(s.get_str());
                    else if (s.type() == json_spirit::null_type)
                    {
                        // mark entry for deletion
                        values.push_back("");
                        if (!values[0].find("0x"))
                            values[0] = asString(fromHex(values[0].substr(2)));
                        keysToBeDeleted.push_back(values[0]);
                    }
                    else
                        ::testing::AssertionFailure() << "Bad type (expected string)";
                }
                
                EXPECT_TRUE(values.size() == 2);
                ss.push_back(make_pair(values[0], values[1]));
                if (!ss.back().first.find("0x"))
                    ss.back().first = asString(fromHex(ss.back().first.substr(2)));
                if (!ss.back().second.find("0x"))
                    ss.back().second = asString(fromHex(ss.back().second.substr(2)));
            }
            
            StateCacheDB m;
            EnforceRefs r(m, true);
            GenericTrieDB<StateCacheDB> t(&m);
            StateCacheDB hm;
            EnforceRefs hr(hm, true);
            HashedGenericTrieDB<StateCacheDB> ht(&hm);
            StateCacheDB fm;
            EnforceRefs fr(fm, true);
            FatGenericTrieDB<StateCacheDB> ft(&fm);
            t.init();
            ht.init();
            ft.init();
            EXPECT_TRUE(t.check(true));
            EXPECT_TRUE(ht.check(true));
            EXPECT_TRUE(ft.check(true));
            
            for (auto const& k: ss)
            {
                if (find(keysToBeDeleted.begin(), keysToBeDeleted.end(), k.first) != keysToBeDeleted.end() && k.second.empty())
                    t.remove(k.first), ht.remove(k.first), ft.remove(k.first);
                else
                    t.insert(k.first, k.second), ht.insert(k.first, k.second), ft.insert(k.first, k.second);
                EXPECT_TRUE(t.check(true));
                EXPECT_TRUE(ht.check(true));
                EXPECT_TRUE(ft.check(true));
                auto i = ft.begin();
                auto j = t.begin();
                for (; i != ft.end() && j != t.end(); ++i, ++j)
                {
                    EXPECT_EQ(i == ft.end(), j == t.end());
                    EXPECT_TRUE((*i).first.toBytes() == (*j).first.toBytes());
                    EXPECT_TRUE((*i).second.toBytes() == (*j).second.toBytes());
                }
                EXPECT_EQ(ht.root(), ft.root());
            }
            
            EXPECT_TRUE(!o["root"].is_null());
            EXPECT_EQ(o["root"].get_str(), toHexPrefixed(t.root().asArray()));
        }
    }
    
    h256 stringMapHash256(StringMap const& _s)
    {
        BytesMap bytesMap;
        for (auto const& _v: _s)
            bytesMap.insert(std::make_pair(bytes(_v.first.begin(), _v.first.end()), bytes(_v.second.begin(), _v.second.end())));
        return hash256(bytesMap);
    }
    
    bytes stringMapRlp256(StringMap const& _s)
    {
        BytesMap bytesMap;
        for (auto const& _v: _s)
            bytesMap.insert(std::make_pair(bytes(_v.first.begin(), _v.first.end()), bytes(_v.second.begin(), _v.second.end())));
        return rlp256(bytesMap);
    }
    
    TEST(Trie, moreTrieTests)
    {
        cnote << "Testing Trie more...";
        // More tests...
        {
            StateCacheDB m;
            GenericTrieDB<StateCacheDB> t(&m);
            t.init();    // initialise as empty tree.
            cnote << t;
            cnote << m;
            cnote << t.root();
            cnote << stringMapHash256(StringMap());
            
            t.insert(string("tesz"), string("test"));
            cnote << t;
            cnote << m;
            cnote << t.root();
            cnote << stringMapHash256({{"test", "test"}});
            
            t.insert(string("tesa"), string("testy"));
            cnote << t;
            cnote << m;
            cnote << t.root();
            cnote << stringMapHash256({{"test", "test"}, {"te", "testy"}});
            cnote << t.at(string("test"));
            cnote << t.at(string("te"));
            cnote << t.at(string("t"));
            
            t.remove(string("te"));
            cnote << m;
            cnote << t.root();
            cnote << stringMapHash256({{"test", "test"}});
            
            t.remove(string("test"));
            cnote << m;
            cnote << t.root();
            cnote << stringMapHash256(StringMap());
        }
        {
            StateCacheDB m;
            GenericTrieDB<StateCacheDB> t(&m);
            t.init();    // initialise as empty tree.
            t.insert(string("a"), string("A"));
            t.insert(string("b"), string("B"));
            cnote << t;
            cnote << m;
            cnote << t.root();
            cnote << stringMapHash256({{"b", "B"}, {"a", "A"}});
            bytes r(stringMapRlp256({{"b", "B"}, {"a", "A"}}));
            cnote << RLP(r);
        }
        {
            MemTrie t;
            t.insert("dog", "puppy");
            cnote << hex << t.hash256();
            bytes r(t.rlp());
            cnote << RLP(r);
        }
        {
            MemTrie t;
            t.insert("bed", "d");
            t.insert("be", "e");
            cnote << hex << t.hash256();
            bytes r(t.rlp());
            cnote << RLP(r);
        }
        {
            cnote << hex << stringMapHash256({{"dog", "puppy"}, {"doe", "reindeer"}});
            MemTrie t;
            t.insert("dog", "puppy");
            t.insert("doe", "reindeer");
            cnote << hex << t.hash256();
            bytes r(t.rlp());
            cnote << RLP(r);
            cnote << toHex(t.rlp());
        }
        {
            StateCacheDB m;
            EnforceRefs r(m, true);
            GenericTrieDB<StateCacheDB> d(&m);
            d.init();    // initialise as empty tree.
            MemTrie t;
            StringMap s;
            
            auto add = [&](char const* a, char const* b)
            {
                d.insert(string(a), string(b));
                t.insert(a, b);
                s[a] = b;
                
                cnote << "/n-------------------------------";
                cnote << a << " -> " << b;
                cnote << d;
                cnote << m;
                cnote << d.root();
                cnote << stringMapHash256(s);
                
                EXPECT_TRUE(d.check(true));
                EXPECT_EQ(t.hash256(), stringMapHash256(s));
                EXPECT_EQ(d.root(), stringMapHash256(s));
                for (auto const& i: s)
                {
                    (void)i;
                    EXPECT_EQ(t.at(i.first), i.second);
                    EXPECT_EQ(d.at(i.first), i.second);
                }
            };
            
            auto remove = [&](char const* a)
            {
                s.erase(a);
                t.remove(a);
                d.remove(string(a));
                
                /*cnote << endl << "-------------------------------";
                 cnote << "X " << a;
                 cnote << d;
                 cnote << m;
                 cnote << d.root();
                 cnote << hash256(s);*/
                
                EXPECT_TRUE(d.check(true));
                EXPECT_TRUE(t.at(a).empty());
                EXPECT_TRUE(d.at(string(a)).empty());
                EXPECT_EQ(t.hash256(), stringMapHash256(s));
                EXPECT_EQ(d.root(), stringMapHash256(s));
                for (auto const& i: s)
                {
                    (void)i;
                    EXPECT_EQ(t.at(i.first), i.second);
                    EXPECT_EQ(d.at(i.first), i.second);
                }
            };
            
            add("dogglesworth", "cat");
            add("doe", "reindeer");
            remove("dogglesworth");
            add("horse", "stallion");
            add("do", "verb");
            add("doge", "coin");
            remove("horse");
            remove("do");
            remove("doge");
            remove("doe");
        }
    }

    namespace
    {
        /// Creates a random, printable, word.
        std::string randomWord()
        {
            static std::mt19937_64 s_eng(0);
            std::string ret(std::uniform_int_distribution<size_t>(1, 5)(s_eng), ' ');
            char const n[] = "qwertyuiop";  // asdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890";
            std::uniform_int_distribution<int> d(0, sizeof(n) - 2);
            for (char& c : ret)
                c = n[d(s_eng)];
            return ret;
        }
    }
    
    TEST(Trie, trieLowerBound)
    {
        cnote << "Stress-testing Trie.lower_bound...";
        if (0)
        {
            StateCacheDB dm;
            EnforceRefs e(dm, true);
            GenericTrieDB<StateCacheDB> d(&dm);
            d.init();    // initialise as empty tree.
            for (int a = 0; a < 20; ++a)
            {
                StringMap m;
                for (int i = 0; i < 50; ++i)
                {
                    auto k = randomWord();
                    auto v = toString(i);
                    m[k] = v;
                    d.insert(k, v);
                }
                
                for (auto i: d)
                {
                    auto it = d.lower_bound(i.first);
                    for (auto iit = d.begin(); iit != d.end(); ++iit)
                        if ((*iit).first.toString() >= i.first.toString())
                        {
                            EXPECT_TRUE(it == iit);
                            break;
                        }
                }
                for (unsigned i = 0; i < 100; ++i)
                {
                    auto k = randomWord();
                    auto it = d.lower_bound(k);
                    for (auto iit = d.begin(); iit != d.end(); ++iit)
                        if ((*iit).first.toString() >= k)
                        {
                            EXPECT_TRUE(it == iit);
                            break;
                        }
                }
                
            }
        }
    }
    
    TEST(Trie, hashedLowerBound)
    {
        // get some random keys and their hashes
        std::map<h256, std::string> hashToKey;
        for (int i = 0; i < 10; ++i)
        {
            std::string key = toString(i);
            hashToKey[sha3(key)] = key;
        }
        
        // insert keys into trie
        StateCacheDB memdb;
        FatGenericTrieDB<StateCacheDB> trie(&memdb);
        trie.init();
        
        for (auto const& hashAndKey : hashToKey)
            trie.insert(hashAndKey.second, std::string("value doesn't matter"));
        
        
        // Trie should have the same order of hashed keys as the map.
        // Get some random hashed key to start iteration
        auto itHashToKey = hashToKey.begin();
        ++itHashToKey;
        ++itHashToKey;
        
        // check trie iteration against map iteration
        for (auto itTrie = trie.hashedLowerBound(itHashToKey->first); itTrie != trie.hashedEnd(); ++itTrie, ++itHashToKey)
        {
            // check hashed key
            EXPECT_EQ((*itTrie).first.toBytes(), itHashToKey->first.asBytes());
            // check key
            EXPECT_EQ(itTrie.key(), bytes(itHashToKey->second.begin(), itHashToKey->second.end()));
        }
        
        EXPECT_EQ(itHashToKey, hashToKey.end());
    }
    
    TEST(Trie, trieStress)
    {
        cnote << "Stress-testing Trie...";
        {
            StateCacheDB m;
            StateCacheDB dm;
            EnforceRefs e(dm, true);
            GenericTrieDB<StateCacheDB> d(&dm);
            d.init();    // initialise as empty tree.
            MemTrie t;
            EXPECT_TRUE(d.check(true));
            for (int a = 0; a < 20; ++a)
            {
                StringMap m;
                for (int i = 0; i < 50; ++i)
                {
                    auto k = randomWord();
                    auto v = toString(i);
                    m[k] = v;
                    t.insert(k, v);
                    d.insert(k, v);
                    EXPECT_EQ(stringMapHash256(m), t.hash256());
                    EXPECT_EQ(stringMapHash256(m), d.root());
                    EXPECT_TRUE(d.check(true));
                }
                while (!m.empty())
                {
                    auto k = m.begin()->first;
                    auto v = m.begin()->second;
                    d.remove(k);
                    t.remove(k);
                    m.erase(k);
                    if (!d.check(true))
                    {
                        // cwarn << m;
                        for (auto i: d)
                            cwarn << i.first.toString() << i.second.toString();
                        
                        StateCacheDB dm2;
                        EnforceRefs e2(dm2, true);
                        GenericTrieDB<StateCacheDB> d2(&dm2);
                        d2.init();    // initialise as empty tree.
                        for (auto i: d)
                            d2.insert(i.first, i.second);
                        
                        cwarn << "Good:" << d2.root();
                        //                    for (auto i: dm2.get())
                        //                        cwarn << i.first << ": " << RLP(i.second);
                        d2.debugStructure(cerr);
                        cwarn << "Broken:" << d.root();    // Leaves an extension -> extension (3c1... -> 742...)
                        //                    for (auto i: dm.get())
                        //                        cwarn << i.first << ": " << RLP(i.second);
                        d.debugStructure(cerr);
                        
                        d2.insert(k, v);
                        cwarn << "Pres:" << d2.root();
                        //                    for (auto i: dm2.get())
                        //                        cwarn << i.first << ": " << RLP(i.second);
                        d2.debugStructure(cerr);
                        d2.remove(k);
                        
                        cwarn << "Good?" << d2.root();
                    }
                    EXPECT_TRUE(d.check(true));
                    EXPECT_EQ(stringMapHash256(m), t.hash256());
                    EXPECT_EQ(stringMapHash256(m), d.root());
                }
            }
        }
    }
    
    template<typename Trie> void perfTestTrie(char const* _name)
    {
        for (size_t p = 1000; p != 1000000; p*=10)
        {
            StateCacheDB dm;
            Trie d(&dm);
            d.init();
            cnote << "TriePerf " << _name << p;
            std::vector<h256> keys(1000);
            Timer t;
            size_t ki = 0;
            for (size_t i = 0; i < p; ++i)
            {
                auto k = h256::random();
                auto v = toString(i);
                d.insert(k, v);
                
                if (i % (p / 1000) == 0)
                    keys[ki++] = k;
            }
            cnote << "Insert " << p << "values: " << t.elapsed();
            t.restart();
            for (auto k: keys)
                d.at(k);
            cnote << "Query 1000 values: " << t.elapsed();
            t.restart();
            size_t i = 0;
            for (auto it = d.begin(); i < 1000 && it != d.end(); ++it, ++i)
                *it;
            cnote << "Iterate 1000 values: " << t.elapsed();
            t.restart();
            for (auto k: keys)
                d.remove(k);
            cnote << "Remove 1000 values:" << t.elapsed() << "\n";
        }
    }
    
    TEST(Trie, triePerf)
    {
        //if (test::Options::get().all)
        if(1)
        {
            perfTestTrie<SpecificTrieDB<GenericTrieDB<StateCacheDB>, h256>>("GenericTrieDB");
            perfTestTrie<SpecificTrieDB<HashedGenericTrieDB<StateCacheDB>, h256>>("HashedGenericTrieDB");
            perfTestTrie<SpecificTrieDB<FatGenericTrieDB<StateCacheDB>, h256>>("FatGenericTrieDB");
        }
        else
            clog << "Skipping hive test Crypto/Trie/triePerf. Use --all to run it.\n";
    }
}  // namespace taraxa

int main(int argc, char** argv) {
    dev::LoggingOptions logOptions;
    logOptions.verbosity = dev::VerbositySilent;
    dev::setupLogging(logOptions);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

