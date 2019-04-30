/*
        Modified Testing from StateUnitTests
*/

#include <libdevcore/DBFactory.h>
#include "libethereum/State.h"

#include <gtest/gtest.h>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace db;

namespace taraxa {
    TEST(State, LoadAccountCode)
    {
        Address addr{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
        State s{0};
        s.createContract(addr);
        uint8_t codeData[] = {'c', 'o', 'd', 'e'};
        s.setCode(addr, {std::begin(codeData), std::end(codeData)});
        s.commit(State::CommitBehaviour::RemoveEmptyAccounts);
        
        auto& loadedCode = s.code(addr);
        EXPECT_TRUE(std::equal(
                               std::begin(codeData), std::end(codeData), std::begin(loadedCode)
                               ));
    }
    
    class StateTest : public ::testing::Test {
        protected:
        void SetUp() override {
            // get some random addresses and their hashes
            for (unsigned i = 0; i < addressCount; ++i)
            {
                Address addr{i};
                hashToAddress[sha3(addr)] = addr;
            }
            
            // create accounts in the state
            for (auto const& hashAndAddr : hashToAddress) {
                state.addBalance(hashAndAddr.second, 100);
            }
            
            state.commit(State::CommitBehaviour::RemoveEmptyAccounts);
        }
        
        State state{0};
        unsigned const addressCount = 10;
        std::map<h256, Address> hashToAddress;
    };

    
#if ETH_FATDB // address is only iterable when ETH_FATDB is enabled
    TEST_F(StateTest, addressesReturnsAllAddresses)
    {
        std::pair<State::AddressMap, h256> addressesAndNextKey =
        state.addresses(h256{}, addressCount * 2);
        State::AddressMap addresses = addressesAndNextKey.first;
        
        EXPECT_EQ(addresses.size(), addressCount);
        EXPECT_TRUE(addresses == hashToAddress);
        EXPECT_EQ(addressesAndNextKey.second, h256{});
    }
    
    TEST_F(StateTest, addressesReturnsNoMoreThanRequested)
    {
        int maxResults = 3;
        std::pair<State::AddressMap, h256> addressesAndNextKey = state.addresses(h256{}, maxResults);
        State::AddressMap& addresses = addressesAndNextKey.first;
        h256& nextKey = addressesAndNextKey.second;
        
        EXPECT_EQ(addresses.size(), maxResults);
        auto itHashToAddressEnd = std::next(hashToAddress.begin(), maxResults);
        EXPECT_TRUE(addresses == State::AddressMap(hashToAddress.begin(), itHashToAddressEnd));
        EXPECT_EQ(nextKey, itHashToAddressEnd->first);
        
        // request next chunk
        std::pair<State::AddressMap, h256> addressesAndNextKey2 = state.addresses(nextKey, maxResults);
        State::AddressMap& addresses2 = addressesAndNextKey2.first;
        EXPECT_EQ(addresses2.size(), maxResults);
        auto itHashToAddressEnd2 = std::next(itHashToAddressEnd, maxResults);
        EXPECT_TRUE(addresses2 == State::AddressMap(itHashToAddressEnd, itHashToAddressEnd2));
    }
    
    TEST_F(StateTest, addressesDoesntReturnDeletedInCache)
    {
        // delete some accounts
        unsigned deleteCount = 3;
        auto it = hashToAddress.begin();
        for (unsigned i = 0; i < deleteCount; ++i, ++it)
            state.kill(it->second);
        // don't commmit
        
        std::pair<State::AddressMap, h256> addressesAndNextKey =
        state.addresses(h256{}, addressCount * 2);
        State::AddressMap& addresses = addressesAndNextKey.first;
        EXPECT_EQ(addresses.size(), addressCount - deleteCount);
        EXPECT_TRUE(addresses == State::AddressMap(it, hashToAddress.end()));
    }
    
    TEST_F(StateTest, addressesReturnsCreatedInCache)
    {
        // create some accounts
        unsigned createCount = 3;
        std::map<h256, Address> newHashToAddress;
        for (unsigned i = addressCount; i < addressCount + createCount; ++i)
        {
            Address addr{i};
            newHashToAddress[sha3(addr)] = addr;
        }
        
        // create accounts in the state
        for (auto const& hashAndAddr : newHashToAddress)
            state.addBalance(hashAndAddr.second, 100);
        // don't commmit
        
        std::pair<State::AddressMap, h256> addressesAndNextKey =
        state.addresses(newHashToAddress.begin()->first, addressCount + createCount);
        State::AddressMap& addresses = addressesAndNextKey.first;
        for (auto const& hashAndAddr : newHashToAddress)
            EXPECT_TRUE(addresses.find(hashAndAddr.first) != addresses.end());
    }
#endif

}  // namespace taraxa

int main(int argc, char** argv) {
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
