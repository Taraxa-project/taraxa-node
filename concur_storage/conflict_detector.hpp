/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-02-08 15:04:04
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-02-11 16:59:41
 */

#ifndef CONFLICT_DETECTOR_HPP
#define CONFLICT_DETECTOR_HPP

#include "concur_hash.hpp"

namespace taraxa {

using namespace taraxa::storage;

class Detector {
 public:
  Detector(unsigned degree_exp);
  // conflict if return false
  bool load(mem_addr_t const& constract, mem_addr_t const& storage,
            trx_t const& trx);
  // conflict if return false
  bool store(mem_addr_t const& constract, mem_addr_t const& storage,
             trx_t const& trx);

 private:
  ConflictHash hash_;
};

}  // namespace taraxa

#endif
