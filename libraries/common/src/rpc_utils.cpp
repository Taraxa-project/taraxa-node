#include "common/rpc_utils.hpp"

namespace taraxa::util {

Json::Value mergeJsons(Json::Value&& o1, Json::Value&& o2) {
  for (auto itr = o2.begin(); itr != o2.end(); ++itr) {
    o1[itr.key().asString()] = *itr;
  }
  return o1;
}

}  // namespace taraxa::util