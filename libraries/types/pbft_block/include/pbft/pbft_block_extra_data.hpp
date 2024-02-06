#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/RLP.h>
#include <libdevcore/SHA3.h>
#include <libdevcrypto/Common.h>

#include "common/types.hpp"
#include "dag/dag_block.hpp"
#include "vote/vote.hpp"

namespace taraxa {

/** @addtogroup PBFT
 * @{
 */

class PbftBlockExtraData {
 public:
  PbftBlockExtraData() {}
  PbftBlockExtraData(const uint16_t major_version, const uint16_t minor_version, const uint16_t patch_version,
                     const uint16_t net_version, const std::string node_implementation);
  PbftBlockExtraData(const bytes& data);

  /**
   * @brief Get rlp
   * @return rlp
   */
  bytes rlp() const;

  /**
   * @brief Get JSON
   * @return Json
   */
  Json::Value getJson() const;

 protected:
  uint16_t major_version_;
  uint16_t minor_version_;
  uint16_t patch_version_;
  uint16_t net_version_;
  std::string node_implementation_;
  static constexpr uint32_t kExtraDataMaxSize = 1024;
};

/** @}*/

}  // namespace taraxa