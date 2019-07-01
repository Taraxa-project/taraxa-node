#include <gtest/gtest.h>
#include <libdevcore/Log.h>
#include <libdevcore/db.h>
#include <libdevcore/MemoryDB.h>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>
#include "util.hpp"
#include "vm/TaraxaVM.hpp"

using namespace boost::property_tree;
using namespace dev;
using namespace dev::db;
using namespace std;

namespace taraxa::vm {
using namespace cgo::db;

TEST(VM, call_dummy) {
  setDatabaseKind(DatabaseKind::MemoryDB);
  SimpleStateDBDelegate stateDB("", true);
  const auto& vm = TaraxaVM::fromConfig({
      StateDBConfig{
          vm::DBConfig::fromTaraxaStateDB(stateDB),
          0,
      },
  });
  const auto& result = vm->transitionState({
      decltype(StateTransitionRequest::stateRoot)(
          "0x000000000000000000000000000000000000000000000000000"
          "0000000000000"),
      decltype(StateTransitionRequest::block){
          decltype(Block::coinbase)(
              "0x0000000000000000000000000000000000000064"),
          1,
          0,
          0,
          100000000000,
          h256("0x00000000000000000000000000000000000000000000000000"
               "00000000000000"),
          decltype(Block::transactions){
              {
                  nullopt,
                  decltype(Transaction::from)(
                      "0x0000000000000000000000000000000000000064"),
                  0,
                  0,
                  100000000,
                  0,
                  fromHex("0x6080604052600560005534801561001557600080"
                          "fd5b5060a2806100246000396000f3"
                          "fe6080604052348015"
                          "600f57600080fd5b506004361060325760003560e01"
                          "c806360fe47b11460375780636d4c"
                          "e63c146053575b6000"
                          "80fd5b605160048036036020811015604b57600080f"
                          "d5b5035606b565b005b6059607056"
                          "5b6040805191825251"
                          "9081900360200190f35b600055565b6000549056fea16562"
                          "7a7a723058202077afb60be6"
                          "30674b4b25a1657ffe"
                          "4edea0ac59876c8ca6aaffd5b3f22379330029"),
                  decltype(Transaction::hash)(
                      "0x0000000000000000000000000000"
                      "000000000000000000000000000000000001"),
              },
          },
      },
  });
}

}  // namespace taraxa::vm

int main(int argc, char** argv) {
  TaraxaStackTrace st;
  dev::LoggingOptions logOptions;
  logOptions.verbosity = dev::VerbositySilent;
  dev::setupLogging(logOptions);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}