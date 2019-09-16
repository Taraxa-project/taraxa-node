#ifndef TARAXA_NODE_CONCUR_STORAGE_CONFLICT_DETECTOR_HPP
#define TARAXA_NODE_CONCUR_STORAGE_CONFLICT_DETECTOR_HPP

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
