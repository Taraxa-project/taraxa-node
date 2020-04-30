#include "chain_config.hpp"

#include <sstream>

namespace taraxa::conf::chain_config {
using std::stringstream;

decltype(Default) const Default([] {
  decltype(Default)::val_t ret;
  ret.replay_protection_service_range = 10;
  ret.dag_genesis_block.sign(
      "b7e22d46c1ba94d5e8347b01d137b5c428fcbbeaf0a77fb024cbbf1517656ff00d04f7f2"
      "5be608c321b0d7483c402c294ff46c49b265305d046a52236c0a363701");
  ret.eth.chain_id = -4;
  ret.eth.genesis_accounts["de2b1203d72d3549ee2f733b00b2789414c7cea5"].balance =
      9007199254740991;
  return ret;
});

}  // namespace taraxa::conf::chain_config
